#include <iostream>

#include "ObjectFreeList.h"

struct Test
{
	int a;
	int b;
	short c;

	Test() : a(0xaaaaaaaa), b(0xbbbbbbbb), c(0xcccc) {}
};

int main()
{
	ObjectFreeList<Test> list(100, false);

	Test* ptr = list.alloc();

	return 0;
}