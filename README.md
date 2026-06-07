# COMP 3430 — Assignment 2
Implement a simulator to mimic a multi-threaded CPU scheduler that follows the rules of MLFQ schedulling policy.

**Name:** Jian Yang  
**Student Number:** 8000293  
**Course:** COMP 3430, Section A01  
**Instructor:** Dr. Saulo dos Santos  

---

## Project Structure

```
3430-a2/
├── a2.c
├── Makefile
├── tasks.txt   # workload provided
├── tasks2.txt  # workload created by me (the one used for testing)
└── README.md
```

---



## Step by Step

**1. Compile**
```bash
cd 3430-a2
make
```

**2. Run**
```bash
./a2 4 3200 tasks.txt # 4 "CPUs" With S set to 3200us
```



##  Report
### Average Turnaround Time (µs)

| CPUs | S (µs) | Type 0 | Type 1 | Type 2 | Type 3 |
| ---- | ------ | ------ | ------ | ------ | ------ |
| 1    | 200    | 46953  | 145435 | 646666 | 580148 |
| 1    | 800    | 45973  | 148196 | 653002 | 582115 |
| 1    | 1600   | 47877  | 149461 | 653165 | 577595 |
| 1    | 3200   | 48234  | 149951 | 659143 | 575045 |
| 2    | 200    | 21527  | 71186  | 308846 | 276100 |
| 2    | 800    | 21935  | 71495  | 312131 | 274151 |
| 2    | 1600   | 22342  | 72282  | 312636 | 273974 |
| 2    | 3200   | 22875  | 74939  | 315187 | 271813 |
| 8    | 200    | 5769   | 18946  | 87706  | 81877  |
| 8    | 800    | 6285   | 19354  | 87944  | 81431  |
| 8    | 1600   | 6547   | 19832  | 88826  | 81650  |
| 8    | 3200   | 6464   | 21347  | 88307  | 81002  |

### Average Response Time (µs)

| CPUs | S (µs) | Type 0 | Type 1 | Type 2 | Type 3 |
| ---- | ------ | ------ | ------ | ------ | ------ |
| 1    | 200    | 1110   | 1070   | 1059   | 1074   |
| 1    | 800    | 1085   | 1081   | 1070   | 1084   |
| 1    | 1600   | 1100   | 1078   | 1093   | 1103   |
| 1    | 3200   | 1104   | 1088   | 1100   | 1103   |
| 2    | 200    | 447    | 487    | 489    | 490    |
| 2    | 800    | 486    | 501    | 516    | 503    |
| 2    | 1600   | 526    | 541    | 563    | 547    |
| 2    | 3200   | 518    | 551    | 567    | 550    |
| 8    | 200    | 76     | 83     | 94     | 82     |
| 8    | 800    | 119    | 121    | 120    | 121    |
| 8    | 1600   | 132    | 126    | 136    | 130    |
| 8    | 3200   | 136    | 142    | 147    | 137    |

### Analysis
> **Q1: How does the value of S affect turnaround time and response time? Is the difference in turnaround time and response time what you expected to see as S and the number of CPUs change? Why or why not?**

**_Turnaround time_** is largely unaffected by S. From the data above, if we focus on the cases with 1 cpu, there is no obvious correlation between turnaround time and value of S, for example, for type 0, turnaround time with S = 800 is even lower then turnaround time with S = 200. This is not suprising, because S controls how often tasks get boosted to the topmost queue, but don't necessarily reduce the total time a task needs for CPU to finish.

**_Response time_** is more related to boost time, the above result is a little suprising to me because I was expecting a more obvious correlation, but for fairly small quantity of CPUs this is not as obvious as I had expected, but for 8 CPUs it becomes pretty obvious, with S = 200, response time is much shorter then longer boost time. This is the case because with shorter boost time, CPUs are more likely to start dequeueing tasks from the topmost priority queues, so newly added tasks are faster to get picked up, with longer boost time, CPUs are more likely to be working with tasks in lower priority queues, so newly added tasks may end up waiting longer to be picked up.

**_Num of CPUs_** has the most obvious impact on both turnaround and response time, which makes sense, with more "workers" working on tasks, tasks will get picked up faster, and with more CPUs resources, tasks will get finished faster, with less time waiting for CPU.



> **Q2: How does adjusting the S value in the system affect the turnaround time or response time for long-running and I/O tasks specifically? Does it appear to be highly correlated?**

**_Long-running tasks_**'s turnaround time is supposed to be correlated to boost time, because with shorter boost time, long-running tasks are being boosted to topmost priority queue more frequent, so the turnaround time should be shorter with shorter boot time, this is not too obvious from the data above, but can still see this pattern. For response time it's the same as any type of tasks, with shorter boost time, new added tasks get picked up faster, so shorter boost time means shorter response time.

**_I/O tasks_**'s turnaround time is not correlated to boost time, for example, with 1 cpu, type 3 tasks' turnaround time actually decreased with longer boost time, this is because I/O tasks consume allotment time at a slower pace (they often yield CPU to do I/O), so they are less likely to be moved to lower-priority queue, hence boost time don't really matter to them.