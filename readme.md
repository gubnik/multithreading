# Solution to the original repo's task
Uses my own library `https://github.com/gubnik/mpsc-queue.git`

## Provided solution options:
1. Ring buffer bounded queue
Based on a common slot token protocol, this is a wait-free queue with constant emplace and pull
times.
## Pros
- **Blazingly fast**
- Does not require custom allocation strategy
- Great cache locality
## Cons
- Incredibly space-heavy
- Lossy, i.e. will not push if the queue is full
- Does not have a dropping mechanism for performance sake (dropping oldest is O(n), n == oldest node)

2. Michael-Scott queue, unbounded
Based on a common M&S queue, this is a more primitive version that relies of atomic tail and head
one-directional linked queue. Pull is wait-free, push may way on exchange head.
## Pros
- Is somewhat fast
- Is very simple to implement and relatively platform-independent
- Supports custom allocation strategies
## Cons
- No cache locality guaranteed
- Not wait-free
- Fast producers outpace a single consumer but this is typical for MPSC unbounded
