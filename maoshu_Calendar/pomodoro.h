#ifndef __POMODORO_H
#define __POMODORO_H

// 番茄钟：计时状态机 + 页面绘制
void pomoBegin();        // 初始化(默认停在专注阶段，未开始)
void pomoLoop();         // 主循环调用：计时、阶段结束切换、每秒局部重绘
void drawPomoPage();     // 整页重绘
void pomoClick();        // 单击：开始/暂停
void pomoReset();        // 双击：重置当前阶段
void pomoApplyConfig();  // 网页改了时长后调用，按新时长重置当前阶段

#endif
