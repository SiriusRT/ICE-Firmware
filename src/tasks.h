#pragma once
// tasks.h — RTOS 任务入口

void task_sensor(void* arg);
void task_control(void* arg);
void task_fan(void* arg);
void task_encoder(void* arg);
void task_telemetry(void* arg);
void task_display(void* arg);
