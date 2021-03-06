#include "core/peer.h"
#include "tools/tools.h"
#include "core/constants.h"
#include "crypto/hash.h"

#include <iostream>
#include <utility>
#include <condition_variable>
#include <mutex>

Peer::~Peer() {
	m_connection.terminate();
}

Peer& Peer::operator=(Peer other) {
	std::swap(m_running, other.m_running);
	std::swap(m_handshake, other.m_handshake);
	std::swap(m_node, other.m_node);
	std::swap(m_starter, other.m_starter);
	std::swap(m_messages, other.m_messages);
	std::swap(m_connection, other.m_connection);
	return *this;
}

void Peer::processMessages() {
	while (m_connection.isActive() && !m_messages.empty()) {
		const std::string& next = m_messages[0];
		std::cout << "Processing: " << next << " from: " << m_connection.getName() << '\n';
		std::unique_ptr<Message> msg = parseFromJson(next);
		if (!msg) {
			std::cout << "Peer: " << m_connection.getName() << " sent bad message: "
				<< next << ". Closing connection" << '\n';
			m_connection.terminate();
			return;
		}
		switch(msg->messageType) {
		case MessageType::HELLO:
			processHelloMessage(dynamic_cast<HelloMessage*>(msg.get()));
			break;
		case MessageType::GET_PEERS:
			processGetPeersMessage(dynamic_cast<GetPeersMessage*>(msg.get()));
			break;
		case MessageType::PEERS:
			processPeersMessage(dynamic_cast<PeersMessage*>(msg.get()));
			break;
		case MessageType::OBJECT:
			processObjectMessage(dynamic_cast<ObjectMessage*>(msg.get()));
			break;
		case MessageType::GET_OBJECT:
			processGetObjectMessage(dynamic_cast<GetObjectMessage*>(msg.get()));
			break;
		case MessageType::I_HAVE_OBJECT:
			processIHaveObjectMessage(dynamic_cast<IHaveObjectMessage*>(msg.get()));
			break;
		default:
			std::cout << "[NODE_ERROR]: Could not find message type" << '\n';
			m_connection.terminate();
			return;
		};
		m_messages.erase(m_messages.begin());
	}
}

void Peer::processHelloMessage(const HelloMessage* message) {
	if (m_handshake) {
		std::cout << "Double handshake from: " << getName() << std::endl;
		m_connection.terminate();
		return;
	}
	if (message->version != LATEST_VERSION) {
		std::cout << "Bad version: " << message->version << " from: " << getName() << std::endl;
		m_connection.terminate();
		return;
	}
	m_handshake = true;
	if (m_starter) {
		return;
	}
	m_connection.send(HelloMessage().asJson());
	std::cout << "Sent hello to: " << m_connection.getName() << '\n';
}

void Peer::processGetPeersMessage(const GetPeersMessage* message) {
	if (!m_handshake) {
		std::cout << getName() << " is asking for peers without handshake" << '\n';
		m_connection.terminate();
		return;
	}
	PeersMessage peersMsg;
	peersMsg.peerIps = m_node->getPeerIpsCopy();
	std::string json = peersMsg.asJson();
	std::cout << "Sending peers: " << json << " to: " << m_connection.getName() << '\n';
	m_connection.send(json);
}

void Peer::processIHaveObjectMessage(const IHaveObjectMessage* message) {
	if (!m_handshake) {
		std::cout << getName() << " is broadcasting object without handshake" << '\n';
		m_connection.terminate();
		return;
	}
	if (m_node->objectExists(message->objectId)) {
		return;
	}
	GetObjectMessage getObject;
	getObject.objectId = message->objectId;
	std::string json = getObject.asJson();
	std::cout << "Asking for object: " << json << " from: " << getName() << '\n';
	m_connection.send(json);
}

void Peer::processObjectMessage(const ObjectMessage* message) {
	if (!m_handshake) {
		std::cout << getName() << " is giving an object without handshake" << '\n';
		m_connection.terminate();
		return;
	}
	std::cout << "Recieved object: " << message->object << " from: " << getName() << '\n';
	std::string objectHash = hashing::SHA256AndEncode(message->object);
	if (m_node->addObject(objectHash, message->object)) {
		IHaveObjectMessage msg;
		msg.objectId = std::move(objectHash);
		m_node->broadcastMessage(msg.asJson());
	}
}

void Peer::processGetObjectMessage(const GetObjectMessage* message) {
	if (!m_handshake) {
		std::cout << getName() << " is asking for an object without handshake" << '\n';
		m_connection.terminate();
		return;
	}
	std::string object;
	if (!m_node->getObject(message->objectId, &object)) {
		return;
	}
	ObjectMessage objectMessage;
	objectMessage.object = object;
	std::string json = objectMessage.asJson();
	std::cout << "Sending object: " << json << " to: " << getName() << '\n';
	m_connection.send(json);
}

void Peer::processPeersMessage(const PeersMessage* message) {
	if (!m_handshake) {
		std::cout << getName() << " is sending peers without handshake" << '\n';
		m_connection.terminate();
		return;
	}
	std::cout << "Adding peers: " << message->asJson() << " from: " << m_connection.getName() << '\n';
	m_node->addNewPeers(message->peerIps);
}

void Peer::sendMessage(const std::string& msg) {
	m_connection.send(msg);
}

void Peer::startWork(bool startCommunication) {
	std::mutex queueLock;
	std::condition_variable queueCondition;
	std::cout << "Starting peer: " << getName() << '\n';
	if (!m_connection.isActive()) {
		std::cout << "Connection is dead: " << getName() << '\n';
		m_running = false;
		return;
	}
	m_running = true;
	m_starter = startCommunication;
	if (startCommunication) {
		std::cout << "Starting communications from node: " << getName() << '\n';
		m_connection.send(HelloMessage().asJson());
		m_connection.send(GetPeersMessage().asJson());
	}
	std::thread reader([&]() {
		std::cout << "Starting reading: " << getName() << '\n';
		while (m_connection.isActive()) {
			std::string msg = m_connection.read();
			if (msg.empty()) {
				continue;
			}
			std::vector<std::string> tokens = tools::tokeinize(msg, '\n');
			std::unique_lock<std::mutex> lck(queueLock);
			std::move(tokens.begin(), tokens.end(), std::back_inserter(m_messages));
			queueCondition.notify_all();
		}
	});
	std::cout << "Starting message processing: " << getName() << '\n';
	while (m_connection.isActive()) {
		std::unique_lock<std::mutex> lck(queueLock);
		queueCondition.wait(lck);
		processMessages();
	}
	reader.join();
	m_running = false;
	std::cout << "Peer: " << getName() << " finished" << std::endl;
	m_node->peerFinished(getName());
}