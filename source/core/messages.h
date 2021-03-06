#pragma once

#include "core/constants.h"

#include <memory>
#include <string>
#include <json.hpp>
#include <map>

const std::string MESSAGE_TYPE_HELLO = "hello";
const std::string MESSAGE_TYPE_GET_PEERS = "getpeers";
const std::string MESSAGE_TYPE_PEERS = "peers";
const std::string MESSAGE_TYPE_GET_OBJECT = "getobject";
const std::string MESSAGE_TYPE_I_HAVE_OBJECT = "ihaveobject";
const std::string MESSAGE_TYPE_OBJECT = "object";

const std::string STR_TYPE = "type";
const std::string STR_VERSION = "version";
const std::string STR_AGENT = "agent";
const std::string STR_PEERS = "peers";
const std::string STR_OBJECT_ID = "objectid";
const std::string STR_OBJECT = "object";

enum class MessageType {
	HELLO,
	GET_PEERS,
	PEERS,
	GET_OBJECT,
	OBJECT,
	I_HAVE_OBJECT,
};

const std::map<std::string, MessageType> GET_TYPE = {
	std::make_pair(MESSAGE_TYPE_HELLO, MessageType::HELLO),
	std::make_pair(MESSAGE_TYPE_GET_PEERS, MessageType::GET_PEERS),
	std::make_pair(MESSAGE_TYPE_PEERS, MessageType::PEERS),
	std::make_pair(MESSAGE_TYPE_OBJECT, MessageType::OBJECT),
	std::make_pair(MESSAGE_TYPE_GET_OBJECT, MessageType::GET_OBJECT),
	std::make_pair(MESSAGE_TYPE_I_HAVE_OBJECT, MessageType::I_HAVE_OBJECT),
};

class Message {
public:
	std::string type;
	MessageType messageType;

	Message() = delete;
	Message(const std::string& type) : type(type), messageType(GET_TYPE.at(type)) { }
	Message(const Message& other) : type(other.type) { }
	virtual ~Message() { }

	virtual std::string asJson() const {
		nlohmann::json msg
		{
			{STR_TYPE, type},
		};
		return msg.dump();
	}
};

class HelloMessage : public Message {
public:
	std::string version;
	std::string agent;

	HelloMessage() : HelloMessage(LATEST_VERSION, NODE_NAME) { }
	HelloMessage(const std::string& version, const std::string& agent) :
		Message(MESSAGE_TYPE_HELLO), version(version), agent(agent) { }
	HelloMessage(const HelloMessage& other) : Message(other), version(other.version),
		agent(other.agent) { }
	~HelloMessage() { }

	std::string asJson() const {
		nlohmann::json msg
		{
			{STR_TYPE, type},
			{STR_VERSION, version},
			{STR_AGENT, agent}
		};
		return msg.dump();
	}
};

class GetPeersMessage : public Message {
public:
	GetPeersMessage() : Message(MESSAGE_TYPE_GET_PEERS) { }
};

class PeersMessage : public Message {
public:
	std::vector<std::string> peerIps;

	PeersMessage() : Message(MESSAGE_TYPE_PEERS) { }

	std::string asJson() const {
		nlohmann::json msg
		{
			{STR_TYPE, type},
			{STR_PEERS, peerIps}
		};
		return msg.dump();
	}
};

class GetObjectMessage : public Message {
public:
	std::string objectId;

	GetObjectMessage() : Message(MESSAGE_TYPE_GET_OBJECT) { }

	std::string asJson() const {
		nlohmann::json msg
		{
			{STR_TYPE, type},
			{STR_OBJECT_ID, objectId}
		};
		return msg.dump();
	}
};

class IHaveObjectMessage : public Message {
public:
	std::string objectId;

	IHaveObjectMessage() : Message(MESSAGE_TYPE_I_HAVE_OBJECT) { }

	std::string asJson() const {
		nlohmann::json msg
		{
			{STR_TYPE, type},
			{STR_OBJECT_ID, objectId}
		};
		return msg.dump();
	}
};

class ObjectMessage : public Message {
public:
	std::string object;

	ObjectMessage() : Message(MESSAGE_TYPE_OBJECT) { }

	std::string asJson() const {
		nlohmann::json msg
		{
			{STR_TYPE, type},
			{STR_OBJECT, object}
		};
		return msg.dump();
	}
};

std::unique_ptr<Message> parseHello(const nlohmann::json& json);
std::unique_ptr<Message> parseGetPeers(const nlohmann::json& json);
std::unique_ptr<Message> parsePeers(const nlohmann::json& json);
std::unique_ptr<Message> parseGetObject(const nlohmann::json& json);
std::unique_ptr<Message> parseObject(const nlohmann::json& json);
std::unique_ptr<Message> parseIHaveObject(const nlohmann::json& json);
std::unique_ptr<Message> parseFromJson(const std::string& msg);
