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

## Use nodes index instead of pointers for actives nodes

Iteration delay: 12.9377 ms, Average: 13.2529 ms
Iteration delay: 12.9221 ms, Average: 13.2523 ms
Iteration delay: 12.9503 ms, Average: 13.2517 ms
Iteration delay: 12.9523 ms, Average: 13.2511 ms
Iteration delay: 12.9964 ms, Average: 13.2506 ms
Iteration delay: 13.0052 ms, Average: 13.2501 ms
Iteration delay: 12.9518 ms, Average: 13.2495 ms
Iteration delay: 13.0086 ms, Average: 13.2491 ms
Iteration delay: 12.9882 ms, Average: 13.2237 ms
Iteration delay: 12.9706 ms, Average: 13.1964 ms

## Remove the extra copy of ParticlesState

Iteration delay: 13.0407 ms, Average: 13.2183 ms
Iteration delay: 12.9871 ms, Average: 13.2179 ms
Iteration delay: 13.0134 ms, Average: 13.2175 ms
Iteration delay: 13.0144 ms, Average: 13.2171 ms
Iteration delay: 12.9954 ms, Average: 13.2166 ms
Iteration delay: 13.0312 ms, Average: 13.2163 ms
Iteration delay: 12.9949 ms, Average: 13.2158 ms
Iteration delay: 13.098 ms, Average: 13.2156 ms
Iteration delay: 13.1487 ms, Average: 13.1913 ms
Iteration delay: 13.0595 ms, Average: 13.1646 ms

## Use a reference instead of a copy for particle deformation tensors

Iteration delay: 12.0354 ms, Average: 12.3051 ms
Iteration delay: 12.1331 ms, Average: 12.3048 ms
Iteration delay: 12.0799 ms, Average: 12.3044 ms
Iteration delay: 12.1802 ms, Average: 12.3041 ms
Iteration delay: 12.137 ms, Average: 12.3038 ms
Iteration delay: 12.1482 ms, Average: 12.3035 ms
Iteration delay: 12.1349 ms, Average: 12.3032 ms
Iteration delay: 12.1026 ms, Average: 12.2805 ms
Iteration delay: 12.0871 ms, Average: 12.2554 ms


## Use register instead of stack for particle weight calculation

Iteration delay: 11.5371 ms, Average: 11.4785 ms
Iteration delay: 11.5168 ms, Average: 11.4786 ms
Iteration delay: 11.5512 ms, Average: 11.4788 ms
Iteration delay: 11.5142 ms, Average: 11.4788 ms
Iteration delay: 11.508 ms, Average: 11.4789 ms
Iteration delay: 11.6402 ms, Average: 11.4792 ms
Iteration delay: 11.6863 ms, Average: 11.4796 ms
Iteration delay: 11.5702 ms, Average: 11.4798 ms
Iteration delay: 11.5459 ms, Average: 11.4595 ms
Iteration delay: 11.5414 ms, Average: 11.4364 ms

## Inverse loop order from x, y z to z, y, x

Iteration delay: 11.4082 ms, Average: 11.319 ms
Iteration delay: 11.3453 ms, Average: 11.319 ms
Iteration delay: 11.362 ms, Average: 11.3191 ms
Iteration delay: 11.3671 ms, Average: 11.3192 ms
Iteration delay: 11.3487 ms, Average: 11.3193 ms
Iteration delay: 11.3771 ms, Average: 11.3194 ms
Iteration delay: 11.3247 ms, Average: 11.3194 ms
Iteration delay: 11.3046 ms, Average: 11.3194 ms
Iteration delay: 11.337 ms, Average: 11.2984 ms
Iteration delay: 11.3065 ms, Average: 11.2757 ms

# Merge src/mpm/* into src/mpm.cpp and mpm.hpp

Iteration delay: 11.7025 ms, Average: 11.1599 ms
Iteration delay: 11.3748 ms, Average: 11.1604 ms
Iteration delay: 11.1969 ms, Average: 11.1604 ms
Iteration delay: 11.2084 ms, Average: 11.1605 ms
Iteration delay: 11.2093 ms, Average: 11.1606 ms
Iteration delay: 11.2189 ms, Average: 11.1607 ms
Iteration delay: 11.2498 ms, Average: 11.1609 ms
Iteration delay: 11.1916 ms, Average: 11.161 ms
Iteration delay: 11.2369 ms, Average: 11.117 ms
Iteration delay: 11.1526 ms, Average: 11.0946 ms

# use memset instead of std::fill for zeroing out grid.nodes

Iteration delay: 10.9061 ms, Average: 10.9387 ms
Iteration delay: 10.9791 ms, Average: 10.9387 ms
Iteration delay: 10.9664 ms, Average: 10.9388 ms
Iteration delay: 10.9995 ms, Average: 10.9389 ms
Iteration delay: 10.9471 ms, Average: 10.9389 ms
Iteration delay: 10.9387 ms, Average: 10.9389 ms
Iteration delay: 11.0182 ms, Average: 10.9391 ms
Iteration delay: 10.9534 ms, Average: 10.9391 ms
Iteration delay: 11.0553 ms, Average: 10.9181 ms
Iteration delay: 10.9953 ms, Average: 10.8963 ms

# do not zero out p_weights and p_weights_gradient at each iteration

Iteration delay: 10.8427 ms, Average: 10.8111 ms
Iteration delay: 10.8185 ms, Average: 10.8111 ms
Iteration delay: 10.8516 ms, Average: 10.8112 ms
Iteration delay: 10.8722 ms, Average: 10.8113 ms
Iteration delay: 10.8340 ms, Average: 10.8113 ms
Iteration delay: 10.7938 ms, Average: 10.8113 ms
Iteration delay: 10.8739 ms, Average: 10.8114 ms
Iteration delay: 10.8760 ms, Average: 10.7918 ms
Iteration delay: 10.8053 ms, Average: 10.7700 ms

# Remove unnecessary floor operation in base_position calculation

Iteration delay: 10.9699 ms, Average: 10.7925 ms
Iteration delay: 10.897 ms, Average: 10.7927 ms
Iteration delay: 10.9441 ms, Average: 10.793 ms
Iteration delay: 11.0312 ms, Average: 10.7935 ms
Iteration delay: 10.929 ms, Average: 10.7937 ms
Iteration delay: 10.9169 ms, Average: 10.794 ms
Iteration delay: 10.9866 ms, Average: 10.7944 ms
Iteration delay: 10.9037 ms, Average: 10.7946 ms
Iteration delay: 11.0576 ms, Average: 10.7758 ms
Iteration delay: 10.9785 ms, Average: 10.7546 ms
