// My.Server.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <process.h>

#include "my_pools.h"

apr_pool_t *root;

unsigned int __stdcall test(PVOID pVoid)
{
	apr_pool_t *child;
	apr_pool_create(&child, root);
	void *pBuff = apr_palloc(child, sizeof(int));
	int *pInt = new (pBuff) int(5);
	printf("pInt = %d\n", *pInt);

	apr_pool_t *grandson;
	apr_pool_create(&grandson, child);
	void *pBuff2 = apr_palloc(grandson, sizeof(int));
	int *pInt2 = new (pBuff2) int(15);
	printf("pInt2 = %d\n", *pInt2);

	apr_pool_destory(grandson);

	apr_pool_destory(child);

	return 0;
}

int main()
{
	int rv;
	rv = apr_pool_initialize();
	apr_pool_create(&root, NULL);

	for (int i = 0; i < 100000; i++)
	{
		_beginthreadex(NULL, 0, test, NULL, 0, NULL);
	}

	getchar();

	apr_pool_destory(root);
	apr_pool_terminate();


	
    std::cout << "Hello World!\n";
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
