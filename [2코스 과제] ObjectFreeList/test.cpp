#define PROFILE
#include "profile.h"

#include "ObjectFreeList.h"

struct Test
{
	int a;
	int b;
	short c;

	Test() : a(0xaaaaaaaa), b(0xbbbbbbbb), c(0xcccc) {}
	~Test()
	{
		a = 0;
		b = 0;
		c = 0;
	}
};

int main()
{
	ObjectFreeList<Test, GuardCheck::OFF, IdCheck::OFF, PlacementNew::OFF> list(10000);

	for (int i = 0; i < 10000000; i++)
	{
		PROFILE_SCOPE("ObjectFreeList.alloc()");
		Test* ptr = list.alloc();
		list.free(ptr); 
	}
	
	for (int i = 0; i < 10000000; i++)
	{
		PROFILE_SCOPE("new");
		Test* ptr = new Test;
		delete ptr;
	}

	ProfileDataOutText("result.txt");


	return 0;
}