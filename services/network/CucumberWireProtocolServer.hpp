#ifndef SERVICES_CUCUMBER_WIRE_PROTOCOL_SERVER_HPP
#define SERVICES_CUCUMBER_WIRE_PROTOCOL_SERVER_HPP

#include "infra/stream/StringInputStream.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "infra/syntax/Json.hpp"
#include "infra/syntax/JsonFormatter.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/BoundedString.hpp"
#include "services/network/Connection.hpp"
#include "services/network/SingleConnectionListener.hpp"
#include "infra/util/IntrusiveList.hpp"
#include "infra/util/IntrusiveForwardList.hpp"

namespace services
{
    class StepStorage
    {
    public:
        class Step
            : public infra::IntrusiveList<Step>::NodeType
        {
        public:
            Step(const infra::JsonArray& matchArguments, const infra::JsonArray& invokeArguments, const infra::BoundedString& stepName);

            ~Step(){}

           uint8_t Id();
           void SetId(uint8_t);
           infra::JsonArray MatchArguments();
           void SetMatchArguments(infra::JsonArray arguments);
           infra::JsonArray InvokeArguments();
           void SetInvokeArguments(infra::JsonArray arguments);
           infra::BoundedString StepName();
           void StepName(infra::BoundedString stepName);

           bool ContainsArguments();
           infra::JsonArray ParseArguments(const infra::BoundedString& nameToMatch, infra::BoundedString& arrayBuffer);

        private:
            uint8_t id;
            infra::JsonArray matchArguments;
            infra::JsonArray invokeArguments;
            infra::BoundedString::WithStorage<128> stepName;

            static uint8_t  nrSteps;
            //void Invoke();
            //bool Match();
        };

    public:
        StepStorage();

    public:
        infra::Optional<StepStorage::Step> MatchStep(uint8_t id);
        infra::Optional<StepStorage::Step> MatchStep(const infra::BoundedString& nameToMatch);
        void AddStep(const StepStorage::Step& step);

    private:
        infra::IntrusiveList<Step> stepList;
    };

    class CucumberWireProtocolParser
    {
    public:
        CucumberWireProtocolParser(StepStorage& stepStorage);

        enum RequestType
        {
            step_matches,
            invoke,
            snippet_text,
            begin_scenario,
            end_scenario,
            invalid
        };

    public:
        void ParseRequest(const infra::ByteRange& inputRange);
        bool ContainsArguments(const infra::BoundedString& string);
        infra::JsonArray ParseArguments(infra::BoundedString& inputString);
        infra::Optional<infra::BoundedString> ReformatStepName(const infra::BoundedString& nameToMatch, infra::JsonArray& arguments, infra::BoundedString& bufferString);
        bool MatchName(const infra::BoundedString& nameToMatchString);
        void FormatResponse(infra::DataOutputStream::WithErrorPolicy& stream);
        bool MatchArguments(infra::JsonArray& arguments);

        void SuccessMessage(infra::BoundedString& responseBuffer);
        void FailureMessage(infra::BoundedString& responseBuffer, infra::BoundedConstString failMessage, infra::BoundedConstString exceptionType);

    private:
        StepStorage& stepStorage;

        RequestType requestType;

        infra::JsonObject nameToMatch;
        bool matchResult;

        uint8_t invokeId;
        infra::JsonArray invokeArguments;      
        infra::BoundedString::WithStorage<256> invokeArgumentBuffer;
    };

    class CucumberWireProtocolConnectionObserver
        : public services::ConnectionObserver
    {
    public:
        CucumberWireProtocolConnectionObserver(const infra::ByteRange receiveBuffer, StepStorage& stepStorage);

        // Implementation of ConnectionObserver
        virtual void SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
        virtual void DataReceived() override;        

    private:
        CucumberWireProtocolParser cucumberParser;
        const infra::ByteRange receiveBuffer;
        infra::BoundedVector<uint8_t> receiveBufferVector;
        infra::ConstByteRange dataBuffer;
        
    };

    class CucumberWireProtocolServer
        : public services::SingleConnectionListener
    {
    public:
        template<size_t BufferSize>
        using WithBuffer = infra::WithStorage<CucumberWireProtocolServer, std::array<uint8_t, BufferSize>>;

        CucumberWireProtocolServer(const infra::ByteRange receiveBuffer, services::ConnectionFactory& connectionFactory, uint16_t port, StepStorage& stepStorage);

    private:
        StepStorage& stepStorage;
        const infra::ByteRange receiveBuffer;
        infra::Creator<services::ConnectionObserver, CucumberWireProtocolConnectionObserver, void(services::IPAddress address)> connectionCreator;
    };
}

#endif
