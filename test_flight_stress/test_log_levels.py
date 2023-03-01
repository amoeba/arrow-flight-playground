from itertools import product
from signal import signal, SIGINT
import subprocess
from time import sleep

from client.test_do_put import test_do_put

verbosity_levels = ["ERROR", "INFO", "DEBUG"]
trace_options = ["tcp", "http", "flowctl", "channel", "subchannel", "api"]


# Useless:
# resource_quota
# subchannel_pool
# executor

# verbosity_levels = ["DEBUG"]
# trace_options = ["http"]

class Server:
    def __init__(self, trace_option: str, verbosity_level: str):
        self.trace_option = trace_option
        self.verbosity_level = verbosity_level
        self.container_name = f"flight-trace-{trace_option}-{verbosity_level}"
        self.docker_ps = None
        self.log_output_file = open(f"logs/traces-{trace_option}-{verbosity_level}.log", "w")
    
    def __enter__(self):
        print("starting server")
        self.docker_ps = subprocess.Popen([
            "docker",
            "run",
            "--publish",
            "5001:5000",
            "--env",
            f"GRPC_VERBOSITY={self.verbosity_level}",
            "--env",
            f"GRPC_TRACE={self.trace_option}",
            "--name",
            self.container_name,
            "example-flight-server"
        ], stdout=self.log_output_file, stderr=self.log_output_file)

        # Cleanup on interrupt
        signal(SIGINT, lambda: self.cleanup())

        return self.container_name

    def cleanup(self):
        if self.docker_ps is not None:
            # Kill container
            subprocess.run([
                "docker",
                "kill",
                container_name
            ])

            self.docker_ps.terminate()

            # delete container
            subprocess.run([
                "docker",
                "rm",
                container_name
            ])
        self.log_output_file.close()
    
    def __exit__(self, type, value, traceback):
        self.cleanup()

for verbosity_level, trace_option in product(verbosity_levels, trace_options):
    print(f"Testing: GRPC_VERBOSITY={verbosity_level} GRPC_TRACE={trace_option}")

    with Server(trace_option, verbosity_level) as container_name:
        sleep(2) # wait for startup
        test_do_put(
            method="thread",
            n_workers=2,
            n_clients=2,
            n_requests=2
        )
