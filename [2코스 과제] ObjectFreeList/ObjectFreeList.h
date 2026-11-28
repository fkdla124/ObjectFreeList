#pragma once
#include <new>

enum class GuardCheck { OFF = false, ON = true };
enum class IdCheck { OFF = false, ON = true };
enum class PlacementNew { OFF = false, ON = true };

static inline constexpr unsigned int GUARD_VALUE = 0xFDFDFDFD;

// 가드 4바이트 고정. 4바이트로 생각하고 하드코딩
using GuardType = unsigned int;

// ================================================================================================
// Node 선언
template <typename T, GuardCheck ListGuardCheck = GuardCheck::OFF, IdCheck ListIdCheck = IdCheck::OFF>
struct Node;
// ================================================================================================

// ================================================================================================
template <typename T>
struct Node<T, GuardCheck::OFF, IdCheck::OFF>
{
	Node* next = nullptr;
	T data;

	// 패딩을 고려한 offset
	static constexpr int UserPtrOffset = alignof(T) > sizeof(Node*) ? alignof(T) : sizeof(Node*);
};
// ================================================================================================


// ================================================================================================
template <typename T>
struct Node<T, GuardCheck::ON, IdCheck::OFF>
{
	Node* next = nullptr;
	GuardType frontGuard;
	T data;
	GuardType backGuard;

	// 패딩을 고려한 offset
	static constexpr int UserPtrOffset = (alignof(T) > (sizeof(next) + sizeof(frontGuard))) ? alignof(T) : (sizeof(next) + sizeof(frontGuard));
	static constexpr int FrontGuardOffset = UserPtrOffset - sizeof(GuardType);
	static constexpr int BackGuardOffset = UserPtrOffset + sizeof(T);
};
// ================================================================================================


// ================================================================================================
template <typename T>
struct Node<T, GuardCheck::OFF, IdCheck::ON>
{
	Node* next = nullptr;
	unsigned int listId; // 자신이 속한 프리리스트 넘버, 구분을 위함
	T data;

	static constexpr int UserPtrOffset = (alignof(T) > (sizeof(next) + sizeof(listId))) ? alignof(T) : (sizeof(next) + sizeof(listId));
};
// ================================================================================================

// ================================================================================================
// GuardCheck = true, ListIdCheck = true
template <typename T>
struct Node<T, GuardCheck::ON, IdCheck::ON>
{
	Node* next = nullptr;
	unsigned int listId; // 자신이 속한 프리리스트 넘버, 구분을 위함
	GuardType frontGuard;
	T data;
	GuardType backGuard;

	// 패딩을 고려한 offset
	static constexpr int UserPtrOffset = (alignof(T) > (sizeof(next) + sizeof(listId) + sizeof(frontGuard))) ? alignof(T) : (sizeof(next) + sizeof(listId) + sizeof(frontGuard));
	static constexpr int FrontGuardOffset = UserPtrOffset - sizeof(GuardType);
	static constexpr int BackGuardOffset = UserPtrOffset + sizeof(T);
};
// ================================================================================================






template <typename T, GuardCheck GuardCheck = GuardCheck::OFF, IdCheck IdCheck = IdCheck::OFF, PlacementNew PlacementNew = PlacementNew::OFF>
class ObjectFreeList
{
private:
	using NodeType = Node<T, GuardCheck, IdCheck>;
	NodeType* freeList;

	int _size;
	int _capacity;

	inline static unsigned int listId = 0;

	NodeType* CreateNode()
	{
		if constexpr (PlacementNew == PlacementNew::ON)
		{
			// malloc() nullptr 체크 오류. 성능을 위해 안 할 것임
			#pragma warning(disable:6011)
			NodeType* ptr = (NodeType*)malloc(sizeof(NodeType));

			if constexpr (GuardCheck == GuardCheck::ON)
			{
				*(GuardType*)((char*)ptr + NodeType::FrontGuardOffset) = GUARD_VALUE;
				*(GuardType*)((char*)ptr + NodeType::BackGuardOffset) = GUARD_VALUE;
			}
			if constexpr (IdCheck == IdCheck::ON)
				ptr->listId = this->listId;

			return ptr;
			#pragma warning(default:6011)
		}
		else
		{
			NodeType* ptr = new NodeType();

			if constexpr (GuardCheck == GuardCheck::ON)
			{
				*(GuardType*)((char*)ptr + NodeType::FrontGuardOffset) = GUARD_VALUE;
				*(GuardType*)((char*)ptr + NodeType::BackGuardOffset) = GUARD_VALUE;
			}
			if constexpr (IdCheck == IdCheck::ON)
				ptr->listId = listId;

			return ptr;
		}
	}

	void DeleteNode(NodeType* ptr)
	{
		if constexpr (PlacementNew == PlacementNew::ON)
			free((void*)ptr);
		else
			delete ptr;
	}

public:
	ObjectFreeList() : freeList(nullptr), _size(0), _capacity(0)
	{
		if constexpr (IdCheck == IdCheck::ON)
			listId++;
	}

	ObjectFreeList(int blockNum) : freeList(nullptr), _size(0), _capacity(blockNum < 0 ? 0 : blockNum)
	{
		if constexpr (IdCheck == IdCheck::ON)
			listId++;

		if (_capacity != 0)
			freeList = CreateNode();

		NodeType* curr = freeList;
		for (int i = 0; i < _capacity - 1; i++)
		{
			curr->next = CreateNode();
			curr = curr->next;
		}
	}

	~ObjectFreeList()
	{
		NodeType* curr = freeList;
		for (int i = 0; i < _capacity - _size; i++)
		{
			NodeType* prevNode = curr;
			curr = curr->next;

			delete prevNode;
		}
	}

	int size()
	{
		return _size;
	}

	int capacity()
	{
		return _capacity;
	}

	T* alloc()
	{
		if (freeList == nullptr)
		{
			freeList = CreateNode();
			_capacity++;
		}

		T* ret = &freeList->data;
		freeList = freeList->next;
		_size++;

		if constexpr (PlacementNew == PlacementNew::ON)
			new((void*)ret) T;

		return ret;
	}

	bool free(T* ptr)
	{
		if (ptr == nullptr)
			return false;

		// 가드체크
		if constexpr (GuardCheck == GuardCheck::ON)
		{
			if (*(GuardType*)((char*)ptr - sizeof(GuardType)) != GUARD_VALUE || *(GuardType*)(ptr + 1) != GUARD_VALUE)
				*(volatile int*)nullptr = 0; // crash
		}

		// 리스트 ID 체크
		if constexpr (IdCheck == IdCheck::ON)
		{
			if (((NodeType*)((char*)ptr - NodeType::UserPtrOffset))->listId != listId)
				*(volatile int*)nullptr = 0; // crash
		}

		((NodeType*)((char*)ptr - NodeType::UserPtrOffset))->next = freeList;
		freeList = ((NodeType*)((char*)ptr - NodeType::UserPtrOffset));
		_size--;

		if constexpr (PlacementNew == PlacementNew::ON)
			ptr->~T();

		return true;
	}
};