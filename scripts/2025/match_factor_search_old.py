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
            # Get the last comma-separated value which is storage efficiency
            storage_efficiency = output.strip().replace('%', '').split(',')[-1]
            storage_eff = round(float(storage_efficiency) / 100, 4)
            print(f"Storage efficiency is {storage_eff * 100}%")
            return storage_eff
        else: 
            print(f"Vaultx failed with return code {result.returncode}")
            return result.stderr
        
    except FileNotFoundError:
        print("Vaultx executable not found")
        return None

def exhaustive_fine_tune(best_match_factor, k_value=26, memory=512, search_radius=0.005, step_size=0.0001):
    """
    Perform exhaustive search around the best match factor found by binary search
    Reduced search radius for faster runtime while maintaining accuracy
    """
    print(f"\n--- Starting Exhaustive Fine-Tuning ---")
    print(f"Center: {best_match_factor:.4f}")
    print(f"Search radius: ±{search_radius:.4f}")
    print(f"Step size: {step_size:.4f}")
    
    # Calculate search range
    start = max(0.0, best_match_factor - search_radius)
    end = min(1.0, best_match_factor + search_radius)
    
    # Calculate number of steps
    num_steps = int((end - start) / step_size)
    estimated_time = num_steps * 10  # 10 minutes per run for K34
    print(f"Will test {num_steps} values from {start:.4f} to {end:.4f}")
    print(f"Estimated time for K34: ~{estimated_time} minutes ({estimated_time/60:.1f} hours)")
    
    best_factor = best_match_factor
    best_distance = float('inf')
    
    current = start
    step_count = 0
    
    while current <= end:
        step_count += 1
        if step_count % 10 == 0:  # More frequent progress updates
            progress = (current - start) / (end - start) * 100
            elapsed_time = step_count * 10  # Estimated elapsed time
            remaining_time = (num_steps - step_count) * 10  # Estimated remaining time
            print(f"Progress: {progress:.1f}% ({step_count}/{num_steps}) - Elapsed: {elapsed_time}min, Remaining: ~{remaining_time}min")
        
        storage_efficiency = float(run_vaultx_command(current, k_value, memory))
        distance = abs(1.0 - storage_efficiency)
        
        if distance < best_distance:
            best_distance = distance
            best_factor = current
            print(f"New best: factor={current:.4f}, storage_efficiency={storage_efficiency:.4f}, distance={distance:.4f}")
        
        current += step_size
    
    print(f"\n--- Exhaustive Search Complete ---")
    print(f"Ultimate best match factor: {best_factor:.4f}")
    print(f"Distance from perfect storage efficiency: {best_distance:.4f}")
    
    return best_factor

def fast_binary_search(beginning_match_factor, k_value=26, memory=512):
    """
    Fast binary search that halves the step size each iteration for quick convergence
    CORRECTED: Lower match factors give higher storage efficiency
    """
    print(f"Started fast binary search with {beginning_match_factor} match_factor, K={k_value}, memory={memory}")
    
    match_factor = beginning_match_factor
    match_modifier = 0.1  # Start with larger steps for faster convergence
    min_modifier = 0.0001  # Minimum step size (4-decimal precision)
    max_iterations = 50  # Much fewer iterations since we halve each time
    iteration = 0
    
    # Track the best result found so far
    best_match_factor = match_factor
    best_storage_efficiency = 0.0
    
    print(f"Using fast binary search parameters:")
    print(f"  - Initial step size: {match_modifier}")
    print(f"  - Minimum step size: {min_modifier} (4-decimal precision)")
    print(f"  - Maximum iterations: {max_iterations}")
    print(f"  - Expected runtime: ~{max_iterations * 10} minutes for K34")
    print(f"  - LOGIC: Lower match_factor = Higher storage efficiency")

    while iteration < max_iterations and match_modifier >= min_modifier:
        iteration += 1
        print(f"\n--- Iteration {iteration} ---")
        print(f"Testing match_factor: {match_factor:.4f} (step size: {match_modifier:.4f})")
        
        storage_efficiency = float(run_vaultx_command(match_factor, k_value, memory))
        print(f"Measured storage efficiency: {storage_efficiency:.4f}")
        
        # Track the best result so far
        if storage_efficiency > best_storage_efficiency:
            best_storage_efficiency = storage_efficiency
            best_match_factor = match_factor
            print(f"NEW BEST: match_factor={match_factor:.4f}, storage_efficiency={storage_efficiency:.4f}")
        
        # Check if we found perfect storage efficiency
        if abs(1.0 - storage_efficiency) <= 0.0001:  # 4-decimal tolerance
            print(f"Perfect storage efficiency found! Match factor: {match_factor:.4f}, Storage efficiency: {storage_efficiency:.4f}")
            return match_factor
        elif storage_efficiency > 1.0:
            # Storage efficiency too high (over 100%), increase match factor to reduce it
            match_factor += match_modifier
            match_modifier /= 2  # Halve the step size
            print(f"Storage efficiency > 1.0 (over 100%). Increasing match_factor to {match_factor:.4f}. Next step size: {match_modifier:.4f}")
        elif storage_efficiency < 0:
            print(f"Storage efficiency < 0, which shouldn't happen. Best match factor found: {best_match_factor:.4f}")
            break
        else:
            # Storage efficiency < 1.0, try lower match factor to increase storage efficiency
            match_factor -= match_modifier
            match_modifier /= 2  # Halve the step size
            print(f"Storage efficiency < 1.0. Trying LOWER match_factor: {match_factor:.4f}. Next step size: {match_modifier:.4f}")
        
        # Ensure match_factor stays within valid bounds
        if match_factor > 1.0:
            match_factor = 1.0
            print(f"Match factor capped at 1.0")
        elif match_factor < 0.0:
            match_factor = 0.0001  # Small positive value instead of 0
            print(f"Match factor set to minimum value 0.0001")
    
    if iteration >= max_iterations:
        print(f"Reached maximum iterations ({max_iterations})")
    if match_modifier < min_modifier:
        print(f"Reached minimum step size ({min_modifier:.4f})")
    
    print(f"\n--- Final Result ---")
    print(f"Best match_factor found: {best_match_factor:.4f}")
    print(f"Best storage efficiency achieved: {best_storage_efficiency:.4f}")
    
    # Final verification with the best match factor
    final_storage_efficiency = float(run_vaultx_command(best_match_factor, k_value, memory))
    print(f"Final storage efficiency verification: {final_storage_efficiency:.4f}")
    print(f"Distance from perfect (1.0): {abs(1.0 - final_storage_efficiency):.4f}")
    
    return best_match_factor

def main():
    """Main function to handle command line arguments and run the search"""
    # Check for command line arguments
    k_value = 26  # Default K value
    memory = 512  # Default memory value
    ultra_precision = False  # Default to high precision only
    
    if len(sys.argv) > 1:
        try:
            k_value = int(sys.argv[1])
            print(f"Using K value: {k_value}")
        except ValueError:
            print(f"Error: Invalid K value '{sys.argv[1]}'. Must be an integer.")
            print("Usage: python match_factor_search.py [K_VALUE] [MEMORY] [ultra]")
            print("Example: python match_factor_search.py 34 1024")
            print("Example: python match_factor_search.py 34 1024 ultra  # for maximum precision")
            sys.exit(1)
    else:
        print(f"No K value specified, using default: {k_value}")
    
    if len(sys.argv) > 2:
        try:
            memory = int(sys.argv[2])
            print(f"Using memory: {memory} MB")
        except ValueError:
            print(f"Error: Invalid memory value '{sys.argv[2]}'. Must be an integer.")
            print("Usage: python match_factor_search.py [K_VALUE] [MEMORY] [ultra]")
            print("Example: python match_factor_search.py 34 1024")
            print("Example: python match_factor_search.py 34 1024 ultra  # for maximum precision")
            sys.exit(1)
    else:
        print(f"No memory value specified, using default: {memory} MB")
    
    # Check for ultra-precision mode
    if len(sys.argv) > 3 and sys.argv[3].lower() == 'ultra':
        ultra_precision = True
        print("ULTRA-PRECISION MODE ENABLED")
        print("Warning: This will take significantly longer but provide maximum accuracy!")
    
    # Get initial storage efficiency with match factor 1
    storage_efficiency = run_vaultx_command(1, k_value, memory)
    if storage_efficiency is None:
        print("Failed to get initial storage efficiency, cannot proceed")
        sys.exit(1)
    
    print(f"Initial storage efficiency: {storage_efficiency}")
    beginning_match_factor = storage_efficiency

    # Start fast binary search
    optimal_match_factor = fast_binary_search(beginning_match_factor, k_value, memory)
    
    # Optionally run exhaustive fine-tuning for ultra-precision
    if ultra_precision:
        print(f"\n{'='*50}")
        print("STARTING ULTRA-PRECISION EXHAUSTIVE SEARCH")
        print("This will provide 4-decimal precision with smaller search radius!")
        print(f"{'='*50}")
        
        response = input("Continue with exhaustive search? (y/n): ")
        if response.lower() == 'y':
            optimal_match_factor = exhaustive_fine_tune(optimal_match_factor, k_value, memory)
        else:
            print("Skipping exhaustive search, using fast binary search result.")
    
    print(f"\n{'='*50}")
    print("=== FINAL RESULT ===")
    print(f"Most accurate match factor found: {optimal_match_factor:.4f}")
    if ultra_precision:
        print("Search method: Ultra-precision (fast binary search + exhaustive fine-tuning)")
    else:
        print("Search method: Fast binary search (4-decimal precision, ~500 minutes for K34)")
        print("Note: Add 'ultra' as 3rd argument for even higher precision")
    print(f"{'='*50}")

if __name__ == "__main__":
    main()
