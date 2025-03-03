# Parses the parameters passed to the main function
def read_args(args):
    int_reader = lambda x : int(x)
    float_reader = lambda x : float(x)
    string_reader = lambda x : x
    ensure_trailing_reader = lambda tr : lambda x : x.rstrip(tr) + tr
    array_reader = lambda element_reader : \
        lambda x : [element_reader(z) for z in x.split(',')]

    # Define reader functions for each parameter
    reader_fns = { "num_agents" : int_reader,
                  "bank_name" : string_reader,
                  "solver_type" : string_reader}

    params_dict = dict()
    params_dict["port_start"] = 20000
    try:
        for a in args[1:]:
            tokens = a.split('=')
            params_dict[tokens[0]] = reader_fns[tokens[0]](tokens[1])
    except Exception as e:
        exit(str(e) + "\n\nCommand line format: python run.py "
             "num_agents=(int) bank_name=(string) solver_type=(string) [port_start=(int)]")
    return params_dict

# Creates the SLURM script that runs generate_config_info.py to create config.cfg, then runs the executable.
def create_slurm_job(num_agents, solver_type):

    with open("sbatch_script.sh",'w') as outfile:
        outfile.write(
        f'''#!/bin/bash
#SBATCH -N {num_agents}
#SBATCH -J {solver_type}job
#SBATCH -t 00:05:00
#SBATCH -p pdebug
#SBATCH -o {solver_type}.txt
#SBATCH --ip-isolate=yes

#### Shell commands
python3 generate_config_info.py num_agents={num_agents} node_list=$SLURM_NODELIST port_start=30000 config_filename=config.cfg
echo "SLURM_NODELIST is $SLURM_NODELIST"
echo " "
srun -n{num_agents} ./call_executable.sh
'''
        )
    outfile.close()

# Creates the actual job to run on each SLURM node, which will run an instance of exe_path 
def create_call_exe(exe_path):

    with open("call_executable.sh",'w') as outfile:
        outfile.write(
            f'''#!/bin/bash 
{exe_path} config.cfg $SLURM_NODEID $SLURM_LOCALID 1 
'''
        )

# Sets file permissions and runs the sbatch script submitting the SLURM job.
def dispatch(bank_name):
    subprocess.run(["chmod", "u+x", "call_executable.sh"])
    subprocess.run(["sbatch" ,"-A" ,bank_name, "sbatch_script.sh"])


##############
# Main funtion
##############
        
if __name__=="__main__":
    import sys
    import subprocess
    import os
    params_dict = read_args(sys.argv)
    exe_path = os.getcwd() + "/" + params_dict["solver_type"]
    create_call_exe(exe_path)
    create_slurm_job(params_dict["num_agents"], params_dict["solver_type"] )
    dispatch(params_dict["bank_name"])

