// simulation_logic.h
#ifndef SIMULATION_LOGIC_H
#define SIMULATION_LOGIC_H

void main_process_logic(int num_stations, float avg_arrive_time_lambda, float avg_wash_time, int run_time_seconds);
void car_process_logic(float avg_wash_time);

#endif // SIMULATION_LOGIC_H