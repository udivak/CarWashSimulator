cmake_minimum_required(VERSION 3.30)
project(CarWashSimulator C)

set(CMAKE_C_STANDARD 11)

add_executable(hw4_gemini main.c
        constants.h
        structures.h
        ipc_utils.h
        logging.h
        signal_handler.h
        simulation_logic.h
        utils.h
        utils.c
        logging.c
        ipc_utils.c
        signal_handler.c
        simulation_logic.c)
