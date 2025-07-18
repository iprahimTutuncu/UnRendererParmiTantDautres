## beginning of optimization.md

Iteration delay: 2130.74 ms, Average: 2130.74 ms
Iteration delay: 2762.26 ms, Average: 2446.5 ms
Iteration delay: 2367.8 ms, Average: 2420.27 ms
Iteration delay: 2218.08 ms, Average: 2369.72 ms
Iteration delay: 2387.34 ms, Average: 2373.25 ms
Iteration delay: 2433.39 ms, Average: 2383.27 ms
Iteration delay: 2401.41 ms, Average: 2385.86 ms
Iteration delay: 2765.39 ms, Average: 2433.3 ms
Iteration delay: 2698.98 ms, Average: 2462.82 ms
Iteration delay: 1772.9 ms, Average: 2393.83 ms
Iteration delay: 1743.63 ms, Average: 2334.72 ms
Iteration delay: 1738.43 ms, Average: 2285.03 ms

## branchless N

Iteration delay: 1816.76 ms, Average: 1816.76 ms
Iteration delay: 2403.67 ms, Average: 2110.22 ms
Iteration delay: 2082.22 ms, Average: 2100.89 ms
Iteration delay: 1918.99 ms, Average: 2055.41 ms
Iteration delay: 2081.93 ms, Average: 2060.72 ms
Iteration delay: 2082.43 ms, Average: 2064.33 ms
Iteration delay: 2074.83 ms, Average: 2065.83 ms

## Use reference instead of pointer for MpmGridNode and directly use index instead of copying vec3

Iteration delay: 1531.04 ms, Average: 1531.04 ms
Iteration delay: 2021.46 ms, Average: 1776.25 ms
Iteration delay: 1759.31 ms, Average: 1770.6 ms
Iteration delay: 1628.21 ms, Average: 1735 ms
Iteration delay: 1762.12 ms, Average: 1740.43 ms
Iteration delay: 1761.94 ms, Average: 1744.01 ms
Iteration delay: 1762.61 ms, Average: 1746.67 ms

## use size_t instead of int for node indices

Iteration delay: 1528.53 ms, Average: 1528.53 ms
Iteration delay: 2005.32 ms, Average: 1766.93 ms
Iteration delay: 1740.08 ms, Average: 1757.98 ms
Iteration delay: 1611.42 ms, Average: 1721.34 ms
Iteration delay: 1742.07 ms, Average: 1725.48 ms
Iteration delay: 1742.52 ms, Average: 1728.32 ms
Iteration delay: 1743.31 ms, Average: 1730.46 ms

## Directly use MpmGridNode instead of unique_ptr<MpmGridNode>

Iteration delay: 438.61 ms, Average: 438.61 ms
Iteration delay: 426.551 ms, Average: 432.58 ms
Iteration delay: 426.165 ms, Average: 430.442 ms
Iteration delay: 425.983 ms, Average: 429.327 ms
Iteration delay: 426.283 ms, Average: 428.718 ms
Iteration delay: 426.131 ms, Average: 428.287 ms
Iteration delay: 426.264 ms, Average: 427.998 ms
Iteration delay: 449.687 ms, Average: 430.709 ms
Iteration delay: 426.129 ms, Average: 430.2 ms


## Fix undefined behavior that caused a bug in the simulation making it super slow

Iteration delay: 23.6469 ms, Average: 27.5998 ms
Iteration delay: 23.6548 ms, Average: 27.592 ms
Iteration delay: 46.8213 ms, Average: 27.63 ms
Iteration delay: 46.9925 ms, Average: 27.6682 ms
Iteration delay: 46.8304 ms, Average: 27.7059 ms
Iteration delay: 30.2807 ms, Average: 27.711 ms
Iteration delay: 26.9847 ms, Average: 27.7096 ms
Iteration delay: 27.0268 ms, Average: 27.7082 ms
Iteration delay: 27.0401 ms, Average: 27.6684 ms
Iteration delay: 27.0363 ms, Average: 27.6065 ms


