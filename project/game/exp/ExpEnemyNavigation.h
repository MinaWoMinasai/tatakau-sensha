#pragma once
#include <array>
#include <cmath>
#include <limits>

/// @brief 通行不可の格子を四方向の幅優先探索でたどり、開始セルから目標へ向かう次のセル添字を返す。
/// @param blocked 添字y * Width + xのセルが通行不可ならtrue。
/// @param start 開始セルの添字。
/// @param goal 目標セルの添字。
/// @return 次のセル。開始と目標が同じならそのセル、範囲外・空きセルなし・経路なしなら-1。
/// @note 開始/目標が塞がれていれば、それぞれ格子上の二乗距離が最も近い空きセルへ置き換える。
/// 置き換えた開始セルから探索するため、返値が元の開始セルに隣接するとは限らない。
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
    // 親セルを記録して最短経路を復元する。探索自体は格子の占有情報を読むだけで、アクターを移動しない。
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
