# Car Wash Simulator -> Ehud Vaknin 209479088, Moshe Bercovich 206676850
To run:
1. Open terminal.
2. Go to the root directory of the project.
3. Run: "gcc main.c utils.c logging.c ipc_utils.c signal_handler.c simulation_logic.c -o wash -lm -lpthread"
4. Now, run the executable with the following arguments: ./wash <number of stations> <average arrival time> <average wash time> <run time in seconds>