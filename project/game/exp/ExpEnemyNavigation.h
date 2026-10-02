#pragma once
#include <array>
#include <cmath>
#include <limits>

// Four-way routing around authored room walls. Occupancy is supplied by Stage,
// keeping navigation testable without graphics or actor ownership.
/// @brief 経験値敵次のセルを検索する。
template <int Width, int Height> int FindExpEnemyNextCell(const std::array<bool, Width * Height>& blocked, int start, int goal)
{
    constexpr int count = Width * Height;
    if (start < 0 || start >= count || goal < 0 || goal >= count)
        return -1;
    auto nearestOpen = [&](int from) {
        if (!blocked[from])
            return from;
        int best = -1;
        int score = (std::numeric_limits<int>::max)();
        for (int i = 0; i < count; ++i) {
            if (blocked[i])
                continue;
            const int dx = i % Width - from % Width;
            const int dy = i / Width - from / Width;
            const int distance = dx * dx + dy * dy;
            if (distance < score) {
                score = distance;
                best = i;
            }
        }
        return best;
    };
    start = nearestOpen(start);
    goal = nearestOpen(goal);
    if (start < 0 || goal < 0)
        return -1;
    if (start == goal)
        return goal;
    std::array<int, count> parents;
    parents.fill(-1);
    std::array<int, count> queue{};
    int front = 0, back = 0;
    queue[back++] = start;
    parents[start] = start;
    while (front < back && parents[goal] < 0) {
        const int cell = queue[front++];
        const int x = cell % Width, y = cell / Width;
        const int neighbors[4] = {x > 0 ? cell - 1 : -1, x + 1 < Width ? cell + 1 : -1, y > 0 ? cell - Width : -1,
                                  y + 1 < Height ? cell + Width : -1};
        for (int next : neighbors) {
            if (next < 0 || blocked[next] || parents[next] >= 0)
                continue;
            parents[next] = cell;
            queue[back++] = next;
        }
    }
    if (parents[goal] < 0)
        return -1;
    int next = goal;
    while (parents[next] != start)
        next = parents[next];
    return next;
}
