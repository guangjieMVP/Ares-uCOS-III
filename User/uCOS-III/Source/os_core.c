#include "os.h"
#include "os_cpu.h"

#if 0
void OS_RdyListInit(void) 
{ 
	OS_PRIO i; 
	OS_RDY_LIST *p_rdy_list; 

	for (i=0u; i< OS_CFG_PRIO_MAX; i++) 
	{ 
		 p_rdy_list = &OSRdyList[i]; 
		 p_rdy_list->HeadPtr = (OS_TCB *)0; 
		 p_rdy_list->TailPtr  = (OS_TCB *)0; 
	} 
} 
#endif

void OS_IdleTaskInit(OS_ERR *p_err);
void OS_RdyListInit(void);
void OSInit (OS_ERR *p_err) 
{ 
		OSRunning       =  OS_STATE_OS_STOPPED;              

	/* 初始化两个全局的TCB,用于任务切换 */
		OSTCBCurPtr     = (OS_TCB *)0;                     
		OSTCBHighRdyPtr = (OS_TCB *)0;  
	
  /* 初始化优先级变量 */
		OSPrioCur    = (OS_PRIO)0;
		OSPrioHighRdy = (OS_PRIO)0;
		
		/* 初始化就绪列表 */
		OS_RdyListInit();  
	
		/* 初始化空闲任务 */
		OS_IdleTaskInit(p_err);   //初始化空闲任务
		
		if(*p_err != OS_ERR_NONE)
		{
			return;
		}			
} 

/*启动任务*/
void OSStart(OS_ERR*p_err)
{
	if(OSRunning == OS_STATE_OS_STOPPED)
	{
#if 0   /* 手动配置任务1先运行 */      
		OSTCBHighRdyPtr = OSRdyList[0].HeadPtr;
#endif
		
		/* 寻找最高优先级 */
		OSPrioHighRdy = OS_PrioGetHighest();
		OSPrioCur     = OSPrioHighRdy;
		
		/* 找到最高优先级任务的TCB */
		OSTCBHighRdyPtr = OSRdyList[OSPrioHighRdy].HeadPtr;
		OSTCBCurPtr     = OSTCBHighRdyPtr;
		
		/* 标志系统运行 */
		OSRunning = OS_STATE_OS_RUNNING;  
		
		OSStartHighRdy();  /* 启动任务切换 不会再返回 */
		
		*p_err = OS_ERR_FATAL_RETURN;
	}
	else
	{
		*p_err = OS_STATE_OS_RUNNING;
	}
}

/* 任务切换，实际就是触发PendSV异常，然后在PendSV异常中进行上下文切换 */
void OSSched (void)
{
#if 0  /* 简单的两个任务轮流执行 */
	if( OSTCBCurPtr == OSRdyList[0].HeadPtr )
	{
		OSTCBHighRdyPtr = OSRdyList[1].HeadPtr;
	}
	else
	{
		OSTCBHighRdyPtr = OSRdyList[0].HeadPtr;
	}
#endif

#if 0
	/* 如果当前任务是空闲任务 */
	if(OSTCBCurPtr == &OSIdleTaskTCB)
	{
		if(OSRdyList[0].HeadPtr->TaskDelayTicks == 0) //检查任务1延时是否结束
		{
			OSTCBHighRdyPtr = OSRdyList[0].HeadPtr;    //执行任务1
		}
		else if(OSRdyList[1].HeadPtr->TaskDelayTicks == 0) //检查任务2延时是否结束
		{
			OSTCBHighRdyPtr = OSRdyList[1].HeadPtr;  //执行任务1
		}
		else
		{
			return;
		}
	}
	else 
	{
		if(OSTCBCurPtr == OSRdyList[0].HeadPtr)  //如果当前执行任务是任务1
		{
			if(OSRdyList[1].HeadPtr->TaskDelayTicks == 0) //检查任务2延时是否结束
			{
				OSTCBHighRdyPtr = OSRdyList[1].HeadPtr;    //执行任务2
			}
			else if(OSTCBCurPtr->TaskDelayTicks != 0)    //检查当前任务延时是否结束
			{
				OSTCBHighRdyPtr = &OSIdleTaskTCB;         //延时没结束执行空闲任务
			}
			else
			{
				return;  /* 两个任务都处在延时中，不进行任务切换 */
			}
		}
		else if(OSTCBCurPtr == OSRdyList[1].HeadPtr)
		{
			if(OSRdyList[0].HeadPtr->TaskDelayTicks == 0)
			{
				OSTCBHighRdyPtr = OSRdyList[0].HeadPtr; 
			}
			else if(OSTCBCurPtr->TaskDelayTicks != 0)
			{
				OSTCBHighRdyPtr = &OSIdleTaskTCB;
			}
			else
			{
				return;
			}
		}
	}
	OS_TASK_SW();  //任务切换
#endif
	
	CPU_SR_ALLOC();
	OS_CRITICAL_ENTER();  /* 进入临界区 */
	
	OSPrioHighRdy   = OS_PrioGetHighest();
	OSTCBHighRdyPtr = OSRdyList[OSPrioHighRdy].HeadPtr;
	
	/* 如果最高优先级任务是当前任务 直接返回，不进行任务切换 */
	if(OSTCBHighRdyPtr == OSTCBCurPtr)
	{ /*退出临界区*/
		OS_CRITICAL_EXIT();
		return;
	}
	
	/*退出临界区*/
	OS_CRITICAL_EXIT();
	
	OS_TASK_SW();  /* 任务切换 即触发PendSV异常 */
}

//char flag5 = 0;
/* 空闲任务 */
void OS_IdleTask(void *p_arg)
{
	p_arg = p_arg;
	
	for(;;)
	{
		OSIdleTaskCtr++;
//		flag5 = 0;
//		OSTimeDly(3);
//		flag5 = 1;
//		OSTimeDly(3);
	}
}

/* 空闲任务初始化 */
void OS_IdleTaskInit(OS_ERR *p_err)
{
	/* 初始化空闲任务计数器 */
	OSIdleTaskCtr = (OS_IDLE_CTR)0;
	
	OSTaskCreate ((OS_TCB      *)  &OSIdleTaskTCB, 
	              (OS_TASK_PTR  )  OS_IdleTask, 
	              (void        *)  0,
					  (OS_PRIO      )  (OS_CFG_PRIO_MAX - 1u), //加上优先级参数初始化
	              (CPU_STK     *)  OSCfg_IdleTaskStkBasePtr,
	              (CPU_STK_SIZE )  OSCfg_IdleTaskStkSize,
					  (OS_TICK      )  0,
	              (OS_ERR      *)  p_err);
}

/* 初始化任务就绪列表 */
void OS_RdyListInit(void)
{
	OS_PRIO i;
	OS_RDY_LIST *p_rdy_list;
	
	for(i = 0u;i < OS_CFG_PRIO_MAX;i++)
	{
		p_rdy_list = &OSRdyList[i];
		p_rdy_list->NbrEnries = (OS_OBJ_QTY)0;
		p_rdy_list->HeadPtr   = (OS_TCB *)0;
		p_rdy_list->TailPtr   = (OS_TCB *)0;
	}
}

/* 插入到链表头部 */
void OS_RdyListInsertHead(OS_TCB *p_tcb)
{
	OS_RDY_LIST  *p_rdy_list;
	OS_TCB       *p_tcb2;
	
	/* 获取链表根部 */
	p_rdy_list = &OSRdyList[p_tcb->prio];
	
	/* 链表为空 */
	if(p_rdy_list->NbrEnries == (OS_OBJ_QTY)0)
	{
		p_rdy_list->NbrEnries = (OS_OBJ_QTY)1;
		p_tcb->NextPtr        = (OS_TCB *)0;
		p_tcb->PrevPtr        = (OS_TCB *)0;
		p_rdy_list->HeadPtr   = p_tcb;
		p_rdy_list->TailPtr   = p_tcb;
	}
	else  /* 链表已经有节点 */
	{
		p_rdy_list->NbrEnries++;
		p_tcb->PrevPtr      = (OS_TCB *)0;     //新头节点前项指针置空
		p_tcb->NextPtr      = p_rdy_list->HeadPtr; //新头结点指向旧头结点
		p_tcb2              = p_rdy_list->HeadPtr;  //旧头结点
		p_tcb2->PrevPtr     = p_tcb;      //旧头节点指向新头节点
		p_rdy_list->HeadPtr = p_tcb;   //根节点头部指针指向新头结点
	}
}

/* 插入到聊表尾部 */
void OS_RdyListInsertTail(OS_TCB *p_tcb)
{
	OS_RDY_LIST  *p_rdy_list;
	OS_TCB       *p_tcb2;
	
	/* 获取链表根部 */
	p_rdy_list = &OSRdyList[p_tcb->prio];
	if(p_rdy_list->NbrEnries == (OS_OBJ_QTY)0)
	{
		p_rdy_list->NbrEnries = (OS_OBJ_QTY)1;
		p_tcb->NextPtr = (OS_TCB *)0;
		p_tcb->PrevPtr = (OS_TCB *)0;
		p_rdy_list->HeadPtr = p_tcb;
		p_rdy_list->TailPtr = p_tcb;
	}
	else
	{
		p_rdy_list->NbrEnries++;
		p_tcb->NextPtr      = (OS_TCB *)0;
		p_tcb->PrevPtr      = p_rdy_list->TailPtr;//新尾结点指向旧尾结点
		p_tcb2              = p_rdy_list->TailPtr;  //旧尾结点
		p_tcb2->NextPtr     = p_tcb;  //旧尾结点指向新尾结点
		p_rdy_list->TailPtr = p_tcb;  //根节点尾指针指向新尾结点
	}
}

void OS_RdyListInsert(OS_TCB *p_tcb)
{
	OS_PrioInsert(p_tcb->prio);
	
	if(p_tcb->prio == OSPrioCur)
	{
		/* 如果是当前优先级 插入到链表尾部 */
		OS_RdyListInsertTail(p_tcb);
	}
	else
	{
		/* 否则插入链表头部 */
		OS_RdyListInsertHead(p_tcb);
	}
}



void OS_RdyListMoveHeadToTail(OS_RDY_LIST *p_rdy_list)
{
	OS_TCB *p_tcb_tail;
	OS_TCB *p_tcb_head;
	OS_TCB *p_tcb1;
	OS_TCB *p_tcb2;
	switch(p_rdy_list->NbrEnries)
	{
		case 0:
		case 1:
			break;
		case 2:
			p_tcb_head = p_rdy_list->HeadPtr;  //头结点指针
			p_tcb_tail = p_rdy_list->TailPtr;  //尾结点指针
			p_rdy_list->HeadPtr = p_tcb_tail;  //尾结点变头结点
		  p_rdy_list->TailPtr = p_tcb_head;  //头结点变尾结点
		  p_tcb_tail->NextPtr = p_tcb_head;  //新头结点指旧头结点
			p_tcb_head->PrevPtr = p_tcb_tail;  //旧头结点指向新头结点
			p_tcb_tail->PrevPtr = (OS_TCB *)0;
			p_tcb_head->NextPtr = (OS_TCB *)0;  
		break;
		
		default:
			p_tcb_head = p_rdy_list->HeadPtr;  //头结点指针
			p_tcb_tail = p_rdy_list->TailPtr;  //尾结点指针
			p_tcb1     = p_tcb_head->NextPtr;  //头结点的后项结点
			p_tcb2     = p_tcb_tail->PrevPtr;  //尾结点的前项结点
			
		 /* 尾结点变头结点 */
			p_tcb_head->NextPtr = (OS_TCB *)0;
			p_tcb_tail->PrevPtr = (OS_TCB *)0;		
			p_tcb_head->PrevPtr = p_tcb2; 
			p_tcb_tail->NextPtr = p_tcb1;			
			p_tcb1->PrevPtr = p_tcb_tail;
			p_tcb2->NextPtr = p_tcb_head;
		break;
			
	}
}


void OS_RdyListRemove(OS_TCB *p_tcb)
{
	OS_RDY_LIST * p_rdy_list;
	OS_TCB *p_tcb1;
	OS_TCB *p_tcb2;
	
	p_rdy_list = &OSRdyList[p_tcb->prio];
	
	/* 保存要删除的节点的前后TCB节点 */
	p_tcb1 = p_tcb->NextPtr;
	p_tcb2 = p_tcb->PrevPtr;
	
	/* 删除的是第一个节点 */
	if(p_tcb1 == (OS_TCB *)0)
	{
		if(p_tcb2 == (OS_TCB *)0)
		{
			/* 删除节点 因为是第一个，所以全部初始化成0即可 */
			p_rdy_list->NbrEnries = (OS_OBJ_QTY)0;
			p_rdy_list->HeadPtr   = (OS_TCB *)0;
			p_rdy_list->TailPtr   = (OS_TCB *)0;
			
			
		}
		else /* 该链表不止一个节点 */
		{
			p_rdy_list->NbrEnries--;
			p_tcb2->PrevPtr     = (OS_TCB *)0;
			p_rdy_list->HeadPtr = p_tcb2;
		}
		OS_PrioRemove(p_tcb->prio);  //就绪列表中移除
	}
	else   /* 该链表中不止一个节点 */   
	{
		p_rdy_list->NbrEnries--;
		p_tcb2->NextPtr = p_tcb1; /* 前一个节点指向后一个节点 */
		
		/* 如果删除的是尾结点 */
		if(p_tcb1 == (OS_TCB *)0) 
		{
			p_rdy_list->TailPtr = p_tcb2;
		}
		else  /* 不是尾结点 */
		{
			p_tcb1->PrevPtr = p_tcb2;
		}
		
		OS_PrioRemove(p_tcb->prio);  //就绪列表中移除
		/* 删除的节点断开 */
		p_tcb->NextPtr = (OS_TCB *)0;
		p_tcb->PrevPtr = (OS_TCB *)0;
		
	}
	
}

/* 
****************************************************************************
*                     任务就绪函数
**************************************************************************
*/

void OS_TaskRdy(OS_TCB *p_tcb)
{
	/* 从时基列表删除 */
	OS_TickListRemove(p_tcb);
	
	/* 插入就绪列表 */
	OS_RdyListInsert(p_tcb);
}


#if OS_CFG_SCHED_ROUND_ROBIN_EN > 0u
void OS_ShcedRoundRobin(OS_RDY_LIST *p_rdy_list)
{
	OS_TCB *p_tcb;
	CPU_SR_ALLOC();
	
	/* 进入临界区 */
	CPU_CRITICAL_ENTER();
	
	p_tcb = p_rdy_list->HeadPtr;
	
	/* 如果TCB节点为空，退出*/
	if(p_tcb == (OS_TCB *)0)
	{
		CPU_CRITICAL_EXIT();
		return;
	}
	/* 如果是空闲任务，也退出 */
	if(p_tcb == &OSIdleTaskTCB)
	{
		CPU_CRITICAL_EXIT();
		return;
	}
	
	/* 时间片自减 */
	if(p_tcb->TimeQuantaCtr > (OS_TICK)0)
	{
		p_tcb->TimeQuantaCtr--;
	}
	
	/* 如果时间没有用完，退出 */
	if(p_tcb->TimeQuantaCtr > (OS_TICK)0)
	{
		CPU_CRITICAL_EXIT();
		return;
	}
	
	/* 如果当前优先级只有一个任务 退出 */
	if(p_rdy_list->NbrEnries < (OS_OBJ_QTY)2)
	{
		CPU_CRITICAL_EXIT();
		return;
	}
	/* 时间片耗完，将任务放到链表的最后一个节点 */
	OS_RdyListMoveHeadToTail(p_rdy_list);
	
	/* 重新获取任务节点 */
	p_tcb = p_rdy_list->HeadPtr;
	
	/* 重载默认的时间片计数值 */
	p_tcb->TimeQuantaCtr = p_tcb->TimeQuanta;
	
	/* 退出临界段 */
	CPU_CRITICAL_EXIT();
}
#endif



