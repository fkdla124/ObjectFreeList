#pragma once

template <typename T>
struct Node
{
	T data;
	Node<T>* next;
};

template <typename T>
class ObjectFreeList
{
private:
	Node<T>* freeList;

	int _size;
	int _capacity;
	bool _placementNew;

public:
	ObjectFreeList() : freeList(nullptr), _size(0), _capacity(0), _placementNew(false) {}

	ObjectFreeList(int blockNum, bool placementNew) : freeList(nullptr), _size(0), _capacity(blockNum < 0 ? 0 : blockNum), _placementNew(placementNew)
	{
		if (_capacity != 0)
			freeList = new Node<T>;

		Node<T>* curr = freeList;
		for (int i = 0; i < _capacity - 1; i++)
		{
			curr->next = new Node<T>;
			curr = curr->next;
		}
	}

	~ObjectFreeList()
	{
		Node<T>* curr = freeList;
		for (int i = 0; i < _capacity - _size; i++)
		{
			Node<T>* prevNode = curr;
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
			freeList = new Node<T>;
			_capacity++;
		}

		T* ret = &freeList->data;
		freeList = freeList->next;
		_size++;

		//if (_placementNew)
		//	new((void*)ret) T;

		return ret;
	}

	bool free(T* ptr)
	{
		if (ptr == nullptr)
			return false;

		reinterpret_cast<Node<T>*>(ptr)->next = freeList;
		freeList = reinterpret_cast<Node<T>*>(ptr);
		_size--;

		//if (_placementNew)
		//	ptr->~T();

		return true;
	}
};