/*
 * Elevator SSTF Look
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
//#include <linux/list.h>

struct sstf_data {
	struct list_head queue;	//Defines the start of the queue
	int dir;		//Defines the request direction
	sector_t head;		//Defines the request head
};

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static int sstf_dispatch(struct request_queue *q, int force)
{
	//Create a list of requests from q.
	struct sstf_data *nd = q->elevator->elevator_data;
	printk("Look Algorithm: sstf_dispatch(); Starting dispatch.\n");

	//Checks if list has nodes.
	if (!list_empty(&nd->queue)) {
		//Create three requests based on queue implementation.
		struct request *rq, *rqNext, *rqPrev;
		
		//rqNext / rqPrev are given request data from adjacent nodes.
		rqNext = list_entry(nd->queue.next, struct request, queuelist);
		rqPrev = list_entry(nd->queue.prev, struct request, queuelist);

		//Evaluate nodes in the list.
		if(rqNext != rqPrev) {
			printk("sstf_dispatch(): Multiple requests detected.\n");
			
			//Check direction.
			if(nd->direction == 0) {
				printk("sstf_dispatch(): Moving backwards.\n");
				
				//Check where rqPext is relative to rq.
				if(nd->head > rqPrev->__sector) {
					//Continue looking backwards.
					rq = rqPrev;
				}

				else {
					//Switch to looking forwards.
					nd->direction = 1;
					rq = rqNext;
				}
			}

			else {
				printk("sstf_dispatch(): Moving forwards.\n");
				
				//Check where rqNext is relative to rq.
				if(nd->head < rqNext->__sector) {
					//Continue looking forwards.
					rq = rqNext;
				}

				else {
					//Switch to looking backwards.
					nd->direction = 0;
					rq = rqPrev;
				}
			}
		}

		else {
			//rqNext == rqPrev; This means there is only one request.
			printk("sstf_dispatch(): Only one request detected.\n");
			rq = rqNext;
		}

		printk("sstf_dispatch(): Request found. Servicing request.\n");
		//rq = list_entry(nd->queue.next, struct request, queuelist);
		
		//Delete serviced request from queue.
		list_del_init(&rq->queuelist);

		//Get new head position.
		nd->head = blk_rq_pos(rq) + blk_rq_sectors(rq);
		
		elv_dispatch_add_tail(q, rq);

		printk("sstf_dispatch(): Finished running.\n");
		printk("sstf_dispatch(): SSTF reading: %llu.\n"
			(unsigned long long rq->__sector));

		//elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;
	struct request *rqNext, *rqPrev;

	printk("Look Algorithm: sstf_add_request() - Starting add request.\n");

	if(list_empty(&nd->queue)) {
		printk("sstf_add_request(): Request list is empty.\n");
		//list_add_tail(&rq->queuelist, &nd->queue);
		list_add(&rq->queuelist, &nd->queue);
	}

	else {
		printk("sstf_add_request(): Looking for open request slot.\n");

		rqNext = list_entry(nd->queue.next, struct request, queuelist);
		rqPrev = list_entry(nd->queue.prev, struct request, queuelist);

		//Iterates through the list to find the best location to add.
		while(blk_rq_pos(rq) > blk_rq_pos(rqNext)) {
			rqNext = list_entry(	rqNext->queuelist.next,
						struct request,
						queuelist);
			rqPrev = list_entry(	rqPrev->queuelist.prev,
						struct request,
						queuelist);
		}

		//Adds request to the proper location in the list.
		list_add(&rq->queuelist, &rqPrev->queuelist);
		printk("sstf_add_request(): Request added correctly.\n");
	}

	printk("Look Algorithm: sstf_add_request() - SSTF adding: %llu.\n",
		(unsigned long long) rq->__sector);
}

static struct request *
sstf_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
sstf_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sstf_data *nd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn		= sstf_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_former_req_fn		= sstf_former_request,
		.elevator_latter_req_fn		= sstf_latter_request,
		.elevator_init_fn		= sstf_init_queue,
		.elevator_exit_fn		= sstf_exit_queue,
	},
	.elevator_name = "look",
	.elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void)
{
	return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Brandon Lee (Adapted by Chase McWhirt)");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO scheduler");


