#pragma once
#include "entity.hpp"

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
	flecs::entity player;
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

namespace bitsery {
	template<typename S>
	void serialize(S& s, std::pair<u32_t, u8_t>& pair) {
		s.value4b(pair.first);
		s.value1b(pair.second);
	}
}

// Allows a shortend version of an entity ID to be sent over
// this reduces message size. It removes any extranous data not needed.
inline u32_t ClearFields(u64_t e) {
	return e & ECS_ENTITY_MASK;
}

class worldSnapshotBuilder_t {
public:
	/**
	 * World snapshot are serialized as so:
	 * Primitive type | Name/Purpose | [] array or none
	 * 
	 * u32_t archetypeCount
	 * // Archetype 1 ex.
	 * u32_t componentTypeCount
	 * u32_t componentTypes[]
	 * u32_t entityCount
	 * u32_t entityId      ] May repeat
	 * unkn componentData1 |
	 * unkn componentData2 ]
	 * // Archetype 2 ex.
	 * u32_t componentTypeCount
	 * u32_t componentTypes[]
	 * u32_t entityCount
	 * u32_t entityId      ] May repeat
	 * unkn componentData1 ]
	 * 
	 * // List of destroyed entities
	 * u32_t entityDestroyCount
	 * u32_t entityId[]
	 * 
	 * // List of destroyed components
	 * u32_t destroyArchetypeCount
	 * // Archetype 1 ex.
	 * u32_t componentTypeCount;
	 * u32_t componentTypes[]
	 * u32_t entityCount;
	 * u32_t entityId[]; // Just a single array 
	 * 
	 * Minimum size guaranteed(of empty snapshot): 4 + 4 + 4 = 12 bytes;
	 * Minimum size guaranteed(of 1 entity 1 component snapshot): 4 + 4 + 4 + 4 + 4 + unkn + 4 + 4 + 4 + 4 + 4 + 4 + 4: 48 bytes 
	 */

	void BuildMessage(flecs::world& world, message_t& message, serializer_t& ser,  networkEcsContext_t& context) {
		std::unordered_map<std::vector<u64_t>, std::vector<u64_t>, u64_hasher_t> archetypes;
		std::unordered_map<std::vector<u64_t>, std::vector<u64_t>, u64_hasher_t> destroyedArchetypes;

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
				message.Serialize(ser, ClearFields(componentTypes[i]));
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
			message.Serialize(ser, ClearFields(destroyedEntities[i]));

			// Reduces message size. Dont include an component destroy update if the entire
			// entity will just be deleted. TODO OPITIMIZE COMPONENT DESTROY UPDATES
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
				message.Serialize(ser, ClearFields(componentTypes[i]));
			}

			// Archetype Data
			message.Serialize(ser, (u32_t)pair.second.size());
			for (u64_t entity : pair.second) {
				message.Serialize(ser, ClearFields(entity));
			}
		}

		// Enabled or Disabled entity

		message.Serialize(ser, (u32_t)enabledOrDisabledEntities.size());
		for(u32_t i = 0; i < enabledOrDisabledEntities.size(); i++) {
			message.Serialize(ser, enabledOrDisabledEntities[i]);
		}

		// clean up
		componentUpdates.clear();
		destroyedEntities.clear();
		componentDestroyed.clear();
	}

	void ResetComponentUpdateQueue() {
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

	void QueueEnableOrDisableUpdate(flecs::entity entity, bool enable) {
		enabledOrDisabledEntities.push_back(std::pair(ClearFields(entity.id()), (u8_t)enable));
	}
private:
	std::unordered_map<u64_t, std::vector<u64_t>> componentUpdates;
	std::unordered_map<u64_t, std::vector<u64_t>> componentDestroyed;
	std::vector<uint64_t> destroyedEntities;
	std::vector<std::pair<u32_t, u8_t>> enabledOrDisabledEntities;
};

struct worldSnapshotBuilderComp_t {
	std::shared_ptr<worldSnapshotBuilder_t> builder;
};

// TOOD, add sync of disabled/enabled entities

class physicsWorldSnapshotBuilder_t {
public:
	physicsWorldSnapshotBuilder_t(flecs::world& world) {
		query = world.query<shapeComp_t>();
	}

	void BuildMessage(physicsWorld_t& physicsWorld, message_t& message, serializer_t& ser, bool forceUpdate = false) {
		for(auto& pair : shapeUpdates) {
			pair.second.clear();
		}

		query.iter([&](flecs::iter& iter, shapeComp_t* shapesArray) {
			for (auto i : iter) {
				shape_t& shape = physicsWorld.GetShape(shapesArray[i].shape);

				if(shape.IsNetworkDirty() || forceUpdate) {
					shapeUpdates[shape.GetType()].push_back(shapesArray[i].shape);
					shape.ResetNetworkDirty();
				}
			}
		});

		message.Serialize(ser, (u32_t)shapeUpdates[shapeEnum_t::polygon].size());
		for(u32_t shapeId : shapeUpdates[shapeEnum_t::polygon]) {
			message.Serialize(ser, shapeId);
			message.Serialize(ser, physicsWorld.GetPolygon(shapeId));
		}
	
		message.Serialize(ser, (u32_t)shapeUpdates[shapeEnum_t::circle].size());
		for (u32_t shapeId : shapeUpdates[shapeEnum_t::circle]) {
			message.Serialize(ser, shapeId);
			message.Serialize(ser, physicsWorld.GetCircle(shapeId));
		}
	}

private:
	std::unordered_map<shapeEnum_t, std::vector<u32_t>> shapeUpdates;
	flecs::query<shapeComp_t> query;
};

template<typename T>
void UpdateIfDirty(flecs::iter& iter, T* dirtys, worldSnapshotBuilderComp_t* snapshotBuilder) {
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

inline void UpdateEntityNetworkDestroy(flecs::iter& iter, size_t i, worldSnapshotBuilderComp_t& snapshotBuilder) {
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
		world.system<T, worldSnapshotBuilderComp_t>().term_at(2).singleton().iter(UpdateIfDirty<T>);
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

	world.component<isNetworked_t>();

	/* Observes entities with isNetworked_t. isNetworked is a special component that essentially keeps track of an entities lifetime.
	   if isNetworked_t is removed from an entity, that entity is considered "dead" and will be removed from all client via a world snapshot .*/
	world.observer<worldSnapshotBuilderComp_t>().term_at(1).singleton().term<isNetworked_t>().event(flecs::OnRemove).each(UpdateEntityNetworkDestroy);

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
	AddNetworkSyncFor<turretComp_t>(world, context);
	AddNetworkSyncFor<score_t>(world, context);
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

