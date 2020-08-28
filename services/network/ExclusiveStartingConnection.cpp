#include "services/network/ExclusiveStartingConnection.hpp"

namespace services
{
    ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnectionFactoryMutex(infra::SharedObjectAllocator<ExclusiveStartingConnection, void(ExclusiveStartingConnectionFactoryMutex& mutex)>& connections)
        : connections(connections)
    {}

    void ExclusiveStartingConnectionFactoryMutex::QueueConnection(WaitingConnection& waitingConnection)
    {
        waitingConnections.push_back(waitingConnection);
        TryAllocateConnection();
    }

    void ExclusiveStartingConnectionFactoryMutex::Started()
    {
        really_assert(starting);
        starting = false;
        TryAllocateConnection();
    }

    void ExclusiveStartingConnectionFactoryMutex::TryAllocateConnection()
    {
        if (!starting && !waitingConnections.empty())
        {
            auto connection = connections.Allocate(*this);
            if (connection != nullptr)
            {
                starting = true;
                auto& waitingConnection = waitingConnections.front();
                waitingConnections.pop_front();
                waitingConnection.Create(connection);
            }
            else
                connections.OnAllocatable([this]() { TryAllocateConnection(); });
        }
    }

    ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnection::ExclusiveStartingConnection(ExclusiveStartingConnectionFactoryMutex& mutex)
        : mutex(mutex)
    {
        mutex.startingConnection = this;
    }

    ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnection::~ExclusiveStartingConnection()
    {
        if (!reportedStarted && mutex.startingConnection == this)
        {
            mutex.startingConnection = nullptr;
            mutex.Started();
        }
    }

    ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnectionRelease::ExclusiveStartingConnectionRelease(ExclusiveStartingConnectionFactoryMutex& mutex)
        : mutex(mutex)
    {
        mutex.startingConnection = nullptr;
    }

    ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnectionRelease::~ExclusiveStartingConnectionRelease()
    {
        if (!reportedStarted)
            mutex.Started();
    }

    void ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnectionRelease::DataReceived()
    {
        if (!reportedStarted)
        {
            reportedStarted = true;
            mutex.Started();
        }

        ConnectionWithHostnameDecorator::DataReceived();
    }

    ExclusiveStartingConnectionFactory::ExclusiveStartingConnectionFactory(infra::BoundedList<infra::NotifyingSharedOptional<Listener>>& listeners, infra::BoundedList<Connector>& connectors,
        ExclusiveStartingConnectionFactoryMutex& mutex, ConnectionFactory& connectionFactory)
        : mutex(mutex)
        , listeners(listeners)
        , connectors(connectors)
        , connectionFactory(connectionFactory)
    {}

    infra::SharedPtr<void> ExclusiveStartingConnectionFactory::Listen(uint16_t port, ServerConnectionObserverFactory& factory, IPVersions versions)
    {
        assert(!listeners.full());
        listeners.emplace_back();
        auto& index = listeners.back();
        listeners.back().OnAllocatable([this, &index]() { listeners.remove(index); });
        return listeners.back().Emplace(*this, port, factory, versions);
    }

    void ExclusiveStartingConnectionFactory::Connect(ClientConnectionObserverFactory& factory)
    {
        connectors.emplace_back(*this, factory);
    }

    void ExclusiveStartingConnectionFactory::CancelConnect(ClientConnectionObserverFactory& factory)
    {
        for (auto& connector : connectors)
            if (connector.CancelConnect(factory))
            {
                connectors.remove(connector);
                return;
            }
    }

    ExclusiveStartingConnectionFactory::Listener::Listener(ExclusiveStartingConnectionFactory& connectionFactory, uint16_t port, ServerConnectionObserverFactory& factory, IPVersions versions)
        : connectionFactory(connectionFactory)
        , factory(factory)
        , listener(connectionFactory.connectionFactory.Listen(port, *this, versions))
    {}

    void ExclusiveStartingConnectionFactory::Listener::ConnectionAccepted(infra::AutoResetFunction<void(infra::SharedPtr<ConnectionObserver> connectionObserver)>&& createdObserver, IPAddress address)
    {
        assert(this->createdObserver == nullptr);

        this->createdObserver = std::move(createdObserver);
        this->address = address;
        connectionFactory.mutex.QueueConnection(*this);
    }

    void ExclusiveStartingConnectionFactory::Listener::Create(const infra::SharedPtr<ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnection>& connection)
    {
        this->connection = connection;
        factory.ConnectionAccepted([this](infra::SharedPtr<ConnectionObserver> connectionObserver)
        {
            this->createdObserver(this->connection);
            if (this->connection->ConnectionObserver::IsAttached())
                this->connection->Attach(connectionObserver);
            this->connection = nullptr;
        }, address);
    }

    ExclusiveStartingConnectionFactory::Connector::Connector(ExclusiveStartingConnectionFactory& connectionFactory, ClientConnectionObserverFactory& clientFactory)
        : connectionFactory(connectionFactory)
        , clientFactory(clientFactory)
    {
        connectionFactory.connectionFactory.Connect(*this);
    }

    bool ExclusiveStartingConnectionFactory::Connector::CancelConnect(ClientConnectionObserverFactory& factory)
    {
        if (&factory == &clientFactory)
        {
            connectionFactory.connectionFactory.CancelConnect(*this);
            return true;
        }

        return false;
    }

    IPAddress ExclusiveStartingConnectionFactory::Connector::Address() const
    {
        return clientFactory.Address();
    }

    uint16_t ExclusiveStartingConnectionFactory::Connector::Port() const
    {
        return clientFactory.Port();
    }

    void ExclusiveStartingConnectionFactory::Connector::ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<ConnectionObserver> connectionObserver)>&& createdObserver)
    {
        this->createdObserver = std::move(createdObserver);
        connectionFactory.mutex.QueueConnection(*this);
    }

    void ExclusiveStartingConnectionFactory::Connector::Create(const infra::SharedPtr<ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnection>& connection)
    {
        this->connection = connection;

        clientFactory.ConnectionEstablished([this](infra::SharedPtr<ConnectionObserver> connectionObserver)
        {
            this->createdObserver(this->connection);
            if (this->connection->ConnectionObserver::IsAttached())
                this->connection->Attach(connectionObserver);
            this->connection = nullptr;
            connectionFactory.connectors.remove(*this);
        });
    }

    void ExclusiveStartingConnectionFactory::Connector::ConnectionFailed(ConnectFailReason reason)
    {
        auto& clientFactory = this->clientFactory;
        connectionFactory.connectors.remove(*this);
        clientFactory.ConnectionFailed(reason);
    }

    ExclusiveStartingConnectionReleaseFactory::ExclusiveStartingConnectionReleaseFactory(infra::BoundedList<infra::NotifyingSharedOptional<Listener>>& listeners, infra::BoundedList<Connector>& connectors,
        infra::BoundedList<infra::NotifyingSharedOptional<ExclusiveStartingConnectionFactoryMutex::ExclusiveStartingConnectionRelease>>& connections,
        ExclusiveStartingConnectionFactoryMutex& mutex, ConnectionFactory& connectionFactory)
        : mutex(mutex)
        , listeners(listeners)
        , connectors(connectors)
        , connections(connections)
        , connectionFactory(connectionFactory)
    {}

    infra::SharedPtr<void> ExclusiveStartingConnectionReleaseFactory::Listen(uint16_t port, ServerConnectionObserverFactory& factory, IPVersions versions)
    {
        assert(!listeners.full());
        listeners.emplace_back();
        auto& index = listeners.back();
        listeners.back().OnAllocatable([this, &index]() { listeners.remove(index); });
        return listeners.back().Emplace(*this, port, factory, versions);
    }

    void ExclusiveStartingConnectionReleaseFactory::Connect(ClientConnectionObserverFactory& factory)
    {
        connectors.emplace_back(*this, factory);
    }

    void ExclusiveStartingConnectionReleaseFactory::CancelConnect(ClientConnectionObserverFactory& factory)
    {
        for (auto& connector : connectors)
            if (connector.CancelConnect(factory))
            {
                connectors.remove(connector);
                return;
            }
    }

    ExclusiveStartingConnectionReleaseFactory::Listener::Listener(ExclusiveStartingConnectionReleaseFactory& connectionFactory, uint16_t port, ServerConnectionObserverFactory& factory, IPVersions versions)
        : connectionFactory(connectionFactory)
        , factory(factory)
        , listener(connectionFactory.connectionFactory.Listen(port, *this, versions))
    {}

    void ExclusiveStartingConnectionReleaseFactory::Listener::ConnectionAccepted(infra::AutoResetFunction<void(infra::SharedPtr<ConnectionObserver> connectionObserver)>&& createdObserver, IPAddress address)
    {
        this->createdObserver = std::move(createdObserver);
        factory.ConnectionAccepted([this](const infra::SharedPtr<ConnectionObserver>& connectionObserver)
        {
        	auto& connectionFactory = this->connectionFactory;
            connectionFactory.connections.emplace_back();
            auto& index = connectionFactory.connections.back();
            connectionFactory.connections.back().OnAllocatable([&connectionFactory, &index]() { connectionFactory.connections.remove(index); });
            auto connection = connectionFactory.connections.back().Emplace(connectionFactory.mutex);
            this->createdObserver(connection);
            connection->Attach(connectionObserver);
        }, address);
    }

    ExclusiveStartingConnectionReleaseFactory::Connector::Connector(ExclusiveStartingConnectionReleaseFactory& connectionFactory, ClientConnectionObserverFactory& clientFactory)
        : connectionFactory(connectionFactory)
        , clientFactory(clientFactory)
    {
        connectionFactory.connectionFactory.Connect(*this);
    }

    bool ExclusiveStartingConnectionReleaseFactory::Connector::CancelConnect(ClientConnectionObserverFactory& factory)
    {
        if (&factory == &clientFactory)
        {
            connectionFactory.connectionFactory.CancelConnect(*this);
            return true;
        }

        return false;
    }

    IPAddress ExclusiveStartingConnectionReleaseFactory::Connector::Address() const
    {
        return clientFactory.Address();
    }

    uint16_t ExclusiveStartingConnectionReleaseFactory::Connector::Port() const
    {
        return clientFactory.Port();
    }

    void ExclusiveStartingConnectionReleaseFactory::Connector::ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<ConnectionObserver> connectionObserver)>&& createdObserver)
    {
        this->createdObserver = std::move(createdObserver);
        clientFactory.ConnectionEstablished([this](const infra::SharedPtr<ConnectionObserver>& connectionObserver)
        {
        	auto& connectionFactory = this->connectionFactory;
            connectionFactory.connections.emplace_back();
            auto& index = connectionFactory.connections.back();
            connectionFactory.connections.back().OnAllocatable([&connectionFactory, &index]() { connectionFactory.connections.remove(index); });
            auto connection = connectionFactory.connections.back().Emplace(connectionFactory.mutex);
            this->createdObserver(connection);
            connection->Attach(connectionObserver);

            connectionFactory.connectors.remove(*this);
        });
    }

    void ExclusiveStartingConnectionReleaseFactory::Connector::ConnectionFailed(ConnectFailReason reason)
    {
        auto& clientFactory = this->clientFactory;
        connectionFactory.connectors.remove(*this);
        clientFactory.ConnectionFailed(reason);
    }
}