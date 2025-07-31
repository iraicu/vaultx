import os 
import subprocess
import sys
import shutil

def clean_plots_directory():
    """Remove all files from the plots/ directory"""
    plots_dir = "./plots/"
    
    if os.path.exists(plots_dir):
        print(f"Cleaning plots directory: {plots_dir}")
        try:
            # Remove all files and subdirectories
            for filename in os.listdir(plots_dir):
                file_path = os.path.join(plots_dir, filename)
                if os.path.isfile(file_path) or os.path.islink(file_path):
                    os.unlink(file_path)
                elif os.path.isdir(file_path):
                    shutil.rmtree(file_path)
            print("Plots directory cleaned successfully")
        except Exception as e:
            print(f"Error cleaning plots directory: {e}")
    else:
        print(f"Plots directory {plots_dir} does not exist, creating it...")
        os.makedirs(plots_dir, exist_ok=True)

def run_vaultx_command(match_factor, k_value=26, memory=512): 
    print(f"Running vaultx with match_factor {match_factor}, K={k_value}, memory={memory}")
    
    # Validate memory requirements for K value
    min_memory_mb = 2 ** (k_value - 20)  # Rough estimate
    if memory < min_memory_mb:
        print(f"WARNING: K={k_value} typically needs >{min_memory_mb}MB, you specified {memory}MB")
        print("This may cause OutOfMemory errors!")

    clean_plots_directory()

    cmd = f"./vaultx -a for -f ./plots/ -g ./plots/ -j ./plots/ -t 128 -K {k_value} -m {memory} -W {memory} -M {match_factor} -x true"

    try: 
        result = subprocess.run(cmd,
                                shell=True,
                                capture_output=True,
                                text=True)

        if result.returncode == 0:
            print("VaultX completed successfully")
            output = result.stdout
            match_efficiency = output.strip().replace('%', '').split(',')[-2:-1:]
            match_eff = round(float(match_efficiency[0]) / 100 , 4)
            print(f"Match efficiency is {match_eff} * 100%")
            return match_eff
        else: 
            print(f"Vaultx failed with return code {result.returncode}")
            return result.stderr
        
    except FileNotFoundError:
        print("Vaultx executable not found")
        return None

def binary_search(beginning_match_factor, k_value=26, memory=512):
    print(f"Started binary search with {beginning_match_factor} match_factor, K={k_value}, memory={memory}")
    match_factor = beginning_match_factor

    while True: 
        new_match_factor = float(run_vaultx_command(match_factor, k_value, memory))
        if new_match_factor == 1: 
            print(f"Perfect match factor is found. It is {match_factor}")
            break
        elif new_match_factor > 1:
            if match_factor + 0.01 > 1:
                match_factor = 1
            else:
                match_factor += 0.001
            print(f"Match efficiency is > 1. New match_factor to try: {match_factor}")
        elif new_match_factor < 0: 
            print(f"Match efficiency is < 0, which is impossible. Match factor that gives the maximum possible match efficiency is {match_factor}")
            break
        else:
            match_factor -= 0.01
            print(f"Match efficiency is < 1 and > 0. New match_factor to try: {match_factor}")

def main():
    """Main function to handle command line arguments and run the search"""
    # Check for command line arguments
    k_value = 26  # Default K value
    memory = 512  # Default memory value
    
    if len(sys.argv) > 1:
        try:
            k_value = int(sys.argv[1])
            print(f"Using K value: {k_value}")
        except ValueError:
            print(f"Error: Invalid K value '{sys.argv[1]}'. Must be an integer.")
            print("Usage: python match_factor_search.py [K_VALUE] [MEMORY]")
            print("Example: python match_factor_search.py 34 1024")
            sys.exit(1)
    else:
        print(f"No K value specified, using default: {k_value}")
    
    if len(sys.argv) > 2:
        try:
            memory = int(sys.argv[2])
            print(f"Using memory: {memory} MB")
        except ValueError:
            print(f"Error: Invalid memory value '{sys.argv[2]}'. Must be an integer.")
            print("Usage: python match_factor_search.py [K_VALUE] [MEMORY]")
            print("Example: python match_factor_search.py 34 1024")
            sys.exit(1)
    else:
        print(f"No memory value specified, using default: {memory} MB")
    
    # Get initial efficiency with match factor 1
    match_efficiency = run_vaultx_command(1, k_value, memory)
    if match_efficiency is None:
        print("Failed to get initial efficiency, cannot proceed")
        sys.exit(1)
    
    print(f"Initial match efficiency: {match_efficiency}")
    beginning_match_factor = match_efficiency

    # Start binary search
    binary_search(beginning_match_factor, k_value, memory)

if __name__ == "__main__":
    main()
