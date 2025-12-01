import os 
import subprocess
import sys
import shutil

# This function remains unchanged as it handles file system cleanup.
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

# This function remains unchanged as it executes the external command and extracts SE.
def run_vaultx_command(match_factor, k_value=26, memory=512): 
    print(f"Running vaultx with match_factor {match_factor:.6f}, K={k_value}, memory={memory}") # ADDED .6f precision for print
clean_plots_directory()

    cmd = f"./vaultx -a for -f ./plots/ -g ./plots/ -j ./plots/ -t 128 -K {k_value} -m {memory} -W {memory} -M {match_factor} -x true"

    try: 
        result = subprocess.run(cmd,
                                 shell=True,
                                 capture_output=True,
                                 text=True)

        if result.returncode == 0:
            # print("VaultX completed successfully") # COMMENTED OUT to reduce log noise
            output = result.stdout
            # Get the last comma-separated value which is storage efficiency
            # NOTE: Assuming the output format provides sufficient precision for the storage efficiency.
            try:
                storage_efficiency = output.strip().replace('%', '').split(',')[-1]
                storage_eff = float(storage_efficiency) / 100
                print(f"Storage efficiency is {storage_eff * 100:.6f}%") # ADDED .6f precision for print
                return storage_eff
            except (ValueError, IndexError):
                print("Error parsing storage efficiency from VaultX output.")
                return 0.0 # Return a default value to allow the search to continue
        else: 
            print(f"Vaultx failed with return code {result.returncode}")
            return None # Return None on failure

    except FileNotFoundError:
        print("Vaultx executable not found")
        sys.exit(1) # Exit immediately if the executable is not found

# --- NEW FUNCTION FOR DYNAMIC CONVERGENCE SEARCH ---
def dynamic_convergence_search(beginning_match_factor, k_value=26, memory=512):

    match_factor = beginning_match_factor
    iteration = 0
    PERFECT_SE = 1.0 
    
    # Track the previous state to calculate the dynamic step
    prev_factor = None
    prev_se = None
    
    # PERFORM INITIAL RUN
    current_se = run_vaultx_command(match_factor, k_value, memory)
    iteration += 1

    if current_se is None:
        print("Error: Initial VaultX run failed.")
        return None, iteration
    
    if current_se == PERFECT_SE:
        print(f"\nTarget SE (100%) hit on first attempt!")
        return match_factor, iteration
    
    print(f"Initial match_factor: {match_factor:.6f}, SE: {current_se:.6f}")

    # Set up the second point for the dynamic step calculation
    prev_factor = match_factor
    prev_se = current_se
    # Calculate an initial arbitrary step. 
    initial_step_size = 0.1 
    if current_se > PERFECT_SE:
        # SE is too high (e.g., 102%), need to INCREASE match_factor to reduce SE
        match_factor = prev_factor + initial_step_size
    else: 
        # SE is too low (e.g., 95%), need to DECREASE match_factor to increase SE
        match_factor = prev_factor - initial_step_size

    # Ensure match_factor stays within valid bounds (0.0 to 1.0)
    match_factor = max(0.0001, min(1.0, match_factor))
    
    print(f"Initial Step Size: {initial_step_size:.4f}. Next match_factor: {match_factor:.6f}")


    # DYNAMIC SEARCH LOOP
    while True:
        iteration += 1
        print(f"\n--- Iteration {iteration} ---")

        # Run VaultX with the new factor
        new_se = run_vaultx_command(match_factor, k_value, memory)

        if new_se is None:
            print("Error: VaultX run failed. Halting search.")
            return match_factor, iteration # Return the last factor found

        # Check for perfect storage efficiency
        if new_se == PERFECT_SE:
            print(f"\n{'='*50}")
            print(f"PERFECT STORAGE EFFICIENCY FOUND: {PERFECT_SE * 100}%")
            print(f"Match Factor: {match_factor:.6f}")
            # The exact number of iterations is printed here
            print(f"Iterations Taken: {iteration}")
            print(f"{'='*50}")
            return match_factor, iteration


        # --- DYNAMIC STEP CALCULATION ---
        # Error (Distance from target) at the new point P2
        error = PERFECT_SE - new_se # The distance to cover (positive if SE is too low, negative if too high)

        # Change in SE for the change in factor
        delta_se = new_se - prev_se
        delta_factor = match_factor - prev_factor

        # Safety check: if SE hasn't changed or we've hit max precision, break (equivalent to a min_modifier check)
        # We also check for division by zero
        if abs(delta_se) < 1e-8 or abs(delta_factor) < 1e-8:
            print(f"Warning: Convergence stalled (delta_se={delta_se:.6e}, delta_factor={delta_factor:.6e}). Halting search.")
            return match_factor, iteration

        # Slope (rate of change): slope = delta_se / delta_factor
        # The required step_size_adjustment = error / slope
        # step_size_adjustment = error / (delta_se / delta_factor)
        # step_size_adjustment = error * (delta_factor / delta_se)

        step_size_adjustment = error * (delta_factor / delta_se)

        next_match_factor = match_factor + step_size_adjustment
        print(f"Current Error (Distance from 1.0): {error:.6f}")
        print(f"Calculated Step Adjustment: {step_size_adjustment:.6f}")
        print(f"Next Match Factor (P3): {next_match_factor:.6f}")

        prev_factor = match_factor
        prev_se = new_se
        match_factor = next_match_factor

        # Ensure match_factor stays within valid bounds
        match_factor = max(0.0001, min(1.0, match_factor))

        # Optional: Add a maximum iteration limit for safety against non-convergence in a real-world scenario
        # if iteration > 100: 
        #     print("Halting: Reached maximum safety iterations.")
        #     return match_factor, iteration

# This function remains unchanged as it is not the main search logic.
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

# --- Main function has been updated to call the new search function ---
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
        print("ULTRA-PRECISION MODE ENABLED (Using original exhaustive search logic if enabled)")
        print("Warning: This will take significantly longer but provide maximum accuracy!")
    
    # Get initial storage efficiency with match factor 1
    # NOTE: The initial match factor is used as the starting point for the new search
    storage_efficiency = run_vaultx_command(1, k_value, memory)
    if storage_efficiency is None:
        print("Failed to get initial storage efficiency, cannot proceed")
        sys.exit(1)
    
    print(f"Initial storage efficiency: {storage_efficiency}")
    beginning_match_factor = storage_efficiency

    # Start the new dynamic convergence search
    # UPDATED CALL: The binary search has been replaced with the dynamic search
    optimal_match_factor, total_iterations = dynamic_convergence_search(beginning_match_factor, k_value, memory)
    
    # The 'ultra-precision' part with the exhaustive search still exists, but the 
    # dynamic search is likely more precise and faster. It is kept for completeness.
    if ultra_precision and optimal_match_factor is not None:
        print(f"\n{'='*50}")
        print("STARTING ULTRA-PRECISION EXHAUSTIVE SEARCH")
        print("This will provide 4-decimal precision with smaller search radius!")
        print(f"{'='*50}")

        response = input("Continue with exhaustive search? (y/n): ")
        if response.lower() == 'y':
            optimal_match_factor = exhaustive_fine_tune(optimal_match_factor, k_value, memory)
        else:
            print("Skipping exhaustive search, using dynamic search result.")
    
    if optimal_match_factor is not None:
        print(f"\n{'='*50}")
        print("=== FINAL RESULT ===")
        # The number of iterations is now included in the final printout (Requirement 3)
        print(f"Most accurate match factor found: {optimal_match_factor:.6f}")
        print(f"Total Iterations: {total_iterations}")
        print("Search method: Dynamic Convergence Search (aiming for exact 100.0%)")
        if ultra_precision:
            print("Note: Exhaustive fine-tuning may have run afterwards.")
        print(f"{'='*50}")

if __name__ == "__main__":
    main()
