#pragma once
#include "entity.hpp"

/***
 * Used for all our network IO needs */

/** 1. The IDGAF how it gets there serial/deserial approach
 * State/Snapshot messages may look like this:
 * (u32) entity-component size
 * (u64) entity id: 
 * (u64) component id:
 * (some amount of bytes for the component)
 * (... next entity-component ... )
 * ^^^ above approach becomes stupidly expensive for message size;
 * guarenteed minimum of 20 bytes overhead for ONE entity-component update
 **/

 /** 2. Tables (CHOSEN)
 * (u32) entity-component size
 * (u64) component id + (u64) component id 2:
 * (u64) entityID (comp1) (copm2)
 * (u64) entityID (comp1) (copm2)
 * (u64) entityID (comp1) (copm2)
 * (u64) entityID (comp1) (copm2)
 * (u64) entityID (comp1) (copm2)
 * (... next entity-component ... )
 * ^^^ I think maybe some how this could be improved, but pratically this is the best way.
 * Organize entities with the same compnonent types together. The minimum amount of bytes is the same as 
 * the first approach but as more components must be sent, the "byte" growth is reduced.
 **/

typedef bitsery::InputBufferAdapter<std::vector<u8_t>> inputAdapter_t;
typedef bitsery::OutputBufferAdapter<std::vector<u8_t>> outputAdapter_t;
typedef bitsery::Deserializer<inputAdapter_t> deserializer_t;
typedef bitsery::Serializer<outputAdapter_t> serializer_t;

enum messageFlagBits_t {
	SEND_ALL = 1 << 0
};

struct message_t {
	message_t() {}

	message_t(message_t&& other) noexcept {
		bytes = std::move(other.bytes);
		connection = other.connection;
		flags = other.flags;
	}

	u32_t Size() { return (u32_t)bytes.size(); }
	uint8_t* Data() { return bytes.data(); }

	void SetData(u32_t size, u8_t* data) {
		bytes.resize(size);
		memcpy(bytes.data(), data, size);
	}

	serializer_t StartSerialize() {
		serializer_t ser(bytes);
		return std::move(ser);
	}

	template<typename T>
	static void Serialize(serializer_t& ser, const T& input) {
		ser.object(input);
	}

	void EndSerialize(serializer_t& ser) {
		ser.adapter().flush();
		bytes.resize(ser.adapter().writtenBytesCount());
	}

	deserializer_t StartDeserialize() {
		deserializer_t des(inputAdapter_t(bytes.begin(), bytes.size()));
		return std::move(des);
	}

	template<typename T>
	static void Deserialize(deserializer_t& des, T& output) {
		des.object(output);
	}

	bool EndDeserialization(deserializer_t& des) {
		return des.adapter().isCompletedSuccessfully();
	}

	void SetConnection(HSteamNetConnection connection) {
		this->connection = connection;
	}

	HSteamNetConnection GetConnection() const {
		return connection;
	}

	void SetFlags(uint8_t flags) {
		this->flags = flags;
	}

	bool IsSendAll() {
		return flags & SEND_ALL;
	}

private:
	uint8_t flags = 0;
	HSteamNetConnection connection = 0;
	std::vector<u8_t> bytes;
};

struct client_t {
	u8_t keys;
	sf::Vector2f mouse;
	u32_t playerID;
};

class messageManager_t {
protected:

	void Send(ISteamNetworkingSockets* sockets, HSteamNetConnection connection, void* payload, u32_t size) {
		EResult result = sockets->SendMessageToConnection(connection, payload, size, k_nSteamNetworkingSend_Unreliable, nullptr);
		if (result != k_EResultOK) {
			std::cout << "Failed to send:" << result << '\n';
			exit(EXIT_FAILURE);
		}
	}

public:
	void Init() {
		pollGroup = SteamNetworkingSockets()->CreatePollGroup();
		if (pollGroup == k_HSteamNetPollGroup_Invalid) {
			std::cout << "Failed to create poll group\n";
			exit(EXIT_FAILURE);
		}
	}


	void Update(std::unordered_map<HSteamNetConnection, client_t>& clients, ISteamNetworkingUtils* utils, ISteamNetworkingSockets* sockets) {
		// Send outgoing messages
		while (!outgoing.empty()) {
			message_t message = std::move(outgoing.front());

			if (message.IsSendAll()) {
				for (const std::pair<HSteamNetConnection, client_t>& pair : clients) {
					if (pair.first == message.GetConnection() || pair.first == hostPlayerID)
						continue;

					Send(sockets, pair.first, message.Data(), message.Size());
				}
			}
			else {
				Send(sockets, message.GetConnection(), message.Data(), message.Size());
			}

			outgoing.pop();
		}

		// Get incoming messages;
		do {
			SteamNetworkingMessage_t* message;
			int amount = sockets->ReceiveMessagesOnPollGroup(pollGroup, &message, 1);
			if (amount < 1) {
				break;
			}

			message_t new_message;
			new_message.SetData(message->GetSize(), (u8_t*)message->GetData());
			new_message.SetConnection(message->GetConnection());
			incoming.push(std::move(new_message));

			message->Release();
		} while (true);
	}

	void PushOutgoing(message_t&& message) {
		outgoing.push(std::move(message));
	}

	bool IsIncomingEmpty() {
		return incoming.empty();
	}

	message_t PopIncoming() {
		message_t message = std::move(incoming.front());
		incoming.pop();
		return message;
	}

	void AddConnection(HSteamNetConnection connection) {
		if (!SteamNetworkingSockets()->SetConnectionPollGroup(connection, pollGroup)) {
			std::cout << "Failed to add connection to poll group: " << connection << '\n';
		}
	}

private:
	HSteamNetPollGroup pollGroup = k_HSteamNetPollGroup_Invalid;
	std::queue<message_t> outgoing;
	std::queue<message_t> incoming;
};

/* Used to assist with serial and deserial-ization in components */
class networkEcsContext_t {
public:
	template<typename T>
	void RecordDeserialize(flecs::world& world) {
		deserializeFunctions[world.component<T>()] =
			[](deserializer_t& des, void* output) {
				// we have to be VERY specific with object type here, hence the static_cast
				T* typedObject = (T*)output;
				des.object(static_cast<T&>(*typedObject));
			};
	}

	template<typename T>
	void RecordSerialize(flecs::world& world) {
		serializeFunctions[world.component<T>()] =
			[](serializer_t& ser, void* input) {
			// we have to be VERY specific with object type here, hence the static_cast
				T* typedObject = (T*)input;
				ser.object(static_cast<T&>(*typedObject));
			};
	}

	void Deserialize(deserializer_t& des, u64_t componentType, void* output) {
		if (deserializeFunctions.find(componentType) == deserializeFunctions.end())
			throw std::string("Component type does not have a deserialization recorded");

		deserializeFunctions[componentType](des, output);
	}

	void Serialize(serializer_t& ser, u64_t componentType, void* input) {
		if (serializeFunctions.find(componentType) == serializeFunctions.end())
			throw std::string("Component type does not have a serialization recorded");

		serializeFunctions[componentType](ser, input);
	}

private:
	std::unordered_map<u64_t, std::function<void(deserializer_t& des, void* output)>> deserializeFunctions;
	std::unordered_map<u64_t, std::function<void(serializer_t& des, void* input)>> serializeFunctions;
};

struct u64_hasher_t {
	std::size_t operator()(std::vector<u64_t> const& vec) const {
		std::size_t seed = vec.size();
		for (auto& i : vec) {
			seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
};

struct isNetworked_t {};
struct networkSystem_t {};

class worldSnapshotBuilder_t {
public:
	void BuildMessage(flecs::world& world, message_t& message, serializer_t& ser,  networkEcsContext_t& context) {
		std::unordered_map<std::vector<u64_t>, std::vector<u64_t>, u64_hasher_t> archetypes;
		std::unordered_map<std::vector<u64_t>, std::vector<u64_t>, u64_hasher_t> destroyedArchetypes;

		/** >>>COPIED FROM game.cpp<<<
		  * What is known:
		  * - Size of serialized components is known on both sides of the connection; their >>>SERIALIZED<<< sizes are the same.
		  *
		  * Below is how entity and their components are serialized in messages.
		  *
		  * b - bytes
		  * ukn - unknown byte usage
		  * ... - repeat above pattern
		  *
		  * Byte size | Name
		  * 4b | Entity archetype count
		  *  vvv Archetype table 1 vvv
		  * 4b | Entity component type count
		  * 4b | Component type 1
		  * 4b | Component type 2
		  * 4b | ... May continue ...
		  * 4b | Entiy count
		  * 8b | Entity id
		  * ukn | Component data 1
		  * ukn | Component data 2
		  * ukn | Component data 3
		  * vvv Archetype table 2 vvv
		  * 4b | Entity component type count
		  * 4b | Component type 1
		  * 4b | ... May continue ...
		  * 4b | Entiy count
		  * 8b | Entity id
		  * ukn | Component data 1
		  * 8b | Entity id
		  * ukn | Component data 1
		  */

		// 1. Find how many different archetypes we have, just requires "reversing the map"
		for(auto& pair : componentUpdates) {
			archetypes[pair.second].push_back(pair.first);
		}
		// do the same for destroyed entity-component update

		// 2. Serialize

		// Component updates
		message.Serialize(ser, (u32_t)archetypes.size());

		for(auto& pair : archetypes) {
			// Archetype Header
			const std::vector<u64_t>& componentTypes = pair.first;
			message.Serialize(ser, (u32_t)componentTypes.size()); // entity component type count
			for(u32_t i = 0; i < componentTypes.size(); i++) { // entity component types
				message.Serialize(ser, componentTypes[i]);
			}
			
			// Archetype Data
			message.Serialize(ser, (u32_t)pair.second.size());
			for(u64_t entity : pair.second) {
				message.Serialize(ser, ClearFields(entity));
				
				for(u64_t componentType : componentTypes) {
					context.Serialize(ser, componentType, world.entity(entity).get_mut(componentType));
				}
			}
		}

		// Delete entities
		message.Serialize(ser, (u32_t)destroyedEntities.size());
		for(uint32_t i = 0; i < destroyedEntities.size(); i++) {
			message.Serialize(ser, destroyedEntities[i]);

			// Reduces message size. Dont include an component destroy update if the entire
			// entity will just be deleted
			if(componentDestroyed.find(destroyedEntities[i]) != componentDestroyed.end()) {
				componentDestroyed.erase(destroyedEntities[i]);
			}
		}

		for (auto& pair : componentDestroyed) {
			destroyedArchetypes[pair.second].push_back(pair.first);
		}

		message.Serialize(ser, (u32_t)destroyedArchetypes.size());
		for(auto& pair : destroyedArchetypes) {
			const std::vector<u64_t>& componentTypes = pair.first;
			message.Serialize(ser, (u32_t)componentTypes.size()); // entity component type count
			for (u32_t i = 0; i < componentTypes.size(); i++) { // entity component types
				message.Serialize(ser, componentTypes[i]);
			}

			// Archetype Data
			message.Serialize(ser, (u32_t)pair.second.size());
			for (u64_t entity : pair.second) {
				message.Serialize(ser, ClearFields(entity));
			}
		}

		// clean up
		componentUpdates.clear();
		destroyedEntities.clear();
		componentDestroyed.clear();
	}

	void ResetEntityUpdateQueue() {
		componentUpdates.clear();
	}

	void QueueComponentUpdate(flecs::entity entity, u64_t componentType) {
		if(componentUpdates.find(entity) == componentUpdates.end()) {
			componentUpdates[entity].push_back(componentType);
		} else {
			for(u64_t existingComponentType : componentUpdates[entity]) {
				if(existingComponentType == componentType)
					return;
			} 

			componentUpdates[entity].push_back(componentType);
		}
	}

	void QuickQueueComponentUpdate(flecs::entity entity, u64_t componentType) {
		componentUpdates[entity].push_back(componentType);
	}

	void QueueEntityDestroyedUpdate(flecs::entity entity) {
		destroyedEntities.push_back(entity);

		if(componentUpdates.find(entity) != componentUpdates.end()) {
			componentUpdates.erase(entity);
		}
	}

	void QueueComponentDestroyedUpdate(flecs::entity entity, u64_t componentType) {
		componentDestroyed[entity].push_back(componentType);
	}
private:
	u64_t ClearFields(u64_t e) {
		return e & ECS_ENTITY_MASK;
	}
private:
	std::unordered_map<u64_t, std::vector<u64_t>> componentUpdates;
	std::unordered_map<u64_t, std::vector<u64_t>> componentDestroyed;
	std::vector<uint64_t> destroyedEntities;
};

struct worldSnapshotBuilderComp_t {
	std::shared_ptr<worldSnapshotBuilder_t> builder;
};

template<typename T>
void AlertIfDirtyFor(flecs::iter& iter, T* dirtys, worldSnapshotBuilderComp_t* snapshotBuilder) {
	static_assert(std::is_base_of_v<networked_t, T>);

	for(auto i : iter) {
		T& dirty = dirtys[i];

		if(dirty.dirty) {
			snapshotBuilder->builder->QueueComponentUpdate(iter.entity(i), (u32_t)iter.world().component<T>());
			dirty.dirty = false;
		}
	}
}

// Instead of adding to the list when dirty, here we add ALL networked components of type T regardless if it is dirty
template<typename T>
void UpdateWholeSnapshot(flecs::iter& iter, T* dirtys, worldSnapshotBuilderComp_t* snapshotBuilder) {
	static_assert(std::is_base_of_v<networked_t, T>);

	for (auto i : iter) {
		T& dirty = dirtys[i];

		snapshotBuilder->builder->QuickQueueComponentUpdate(iter.entity(i), iter.world().component<T>());
	}
}

inline void UpdateEntityNetworkDelete(flecs::iter& iter, size_t i, worldSnapshotBuilderComp_t& snapshotBuilder) {
	snapshotBuilder.builder->QueueEntityDestroyedUpdate(iter.entity(i));
}

template<typename T>
inline void UpdateComponentNetworkDestroy(flecs::iter& iter, size_t i, T& component, worldSnapshotBuilderComp_t& shapshotBuilder) {
	shapshotBuilder.builder->QueueComponentDestroyedUpdate(iter.entity(i), iter.world().component<T>());
}

template<typename T>
void AddNetworkSyncFor(flecs::world& world, networkEcsContext_t& context) {
    world.component<T>().is_a<networked_t>();

	context.RecordSerialize<T>(world);
	context.RecordDeserialize<T>(world);

	if(world.has<isHost_t>()) {
		world.system<T, worldSnapshotBuilderComp_t>().term_at(2).singleton().iter(AlertIfDirtyFor<T>);
		world.system<T, worldSnapshotBuilderComp_t>().term_at(2).singleton().kind<networkSystem_t>().iter(UpdateWholeSnapshot<T>);
		world.observer<T, worldSnapshotBuilderComp_t>().term_at(2).singleton().event(flecs::OnRemove).each(UpdateComponentNetworkDestroy<T>); 
	}
}

inline void ImportNetwork(flecs::world& world, networkEcsContext_t& context, std::shared_ptr<worldSnapshotBuilder_t>& worldSnapshotBuilder, flecs::entity& networkPipeline) {
	worldSnapshotBuilder = std::make_shared<worldSnapshotBuilder_t>();
	world.set([&](worldSnapshotBuilderComp_t& builder){
		builder.builder = worldSnapshotBuilder;
	});

	networkPipeline = 
		world.pipeline()
		.with(flecs::System)
		.with<networkSystem_t>()
		.build();

	/* Observes entities with isNetworked_t. isNetworked is a special component that essentially keeps track of a entities lifetime.
	   if isNetworked_t is removed from an entity, that entity is considered "dead" and will be removed from all client via a world snapshot .*/
	world.observer<worldSnapshotBuilderComp_t>().term_at(1).singleton().term<isNetworked_t>().event(flecs::OnRemove).each(UpdateEntityNetworkDelete);

	world.component<isNetworked_t>();

	/* ALL NETWORKED COMPONENTS MUST BE DECLARED HERE! */
	AddNetworkSyncFor<transform_t>(world, context);
	AddNetworkSyncFor<health_t>(world, context);
	AddNetworkSyncFor<integratable_t>(world, context);
	AddNetworkSyncFor<color_t>(world, context);
	AddNetworkSyncFor<playerComp_t>(world, context);
	AddNetworkSyncFor<asteroidComp_t>(world, context);
	AddNetworkSyncFor<sharedLives_t>(world, context);
	AddNetworkSyncFor<bulletComp_t>(world, context);
	AddNetworkSyncFor<mapSize_t>(world, context);
	AddNetworkSyncFor<shapeComp_t>(world, context);
}

// *** NON ECS RELATED NETWORK MESSAGES ***

struct messageInput_t {
	messageInput_t() : input(0) {}
	messageInput_t(uint8_t i, sf::Vector2f mouse) : input(i), mouse(mouse) {}

	uint8_t input;
	sf::Vector2f mouse;

	template<typename S>
	void serialize(S& s) {
		s.value1b(input);
		s.object(mouse);
	}
};

