import math

def calculate_velocities(posA, posB, pulleySpacing, Vx, Vy):
    # Compute the common term used in both velocity equations
    common_term = -math.pow(pulleySpacing, 4) + (2 * posA**2 + 2 * posB**2) * pulleySpacing**2 - math.pow(posA - posB, 2) * math.pow(posA + posB, 2)
    
    if common_term <= 0:
        raise ValueError("Invalid input: common_term must be positive for sqrt operation.")
    
    sqrt_term = math.sqrt(common_term)
    pow_term = math.pow(common_term, -0.5)
    
    # Compute velocities
    velA = pow_term * posA * (Vx * sqrt_term - Vy * (posA**2 - posB**2 - pulleySpacing**2)) / pulleySpacing
    velB = -posB * (Vx * sqrt_term - Vy * (posA**2 - posB**2 + pulleySpacing**2)) * pow_term / pulleySpacing
    
    return velA, velB

# Example usage with sample values:
steps_per_meter = 20308

posA_sample = 18748.00
posB_sample = 24973.00
pulleySpacing_sample = 25791.16

pulley_spacing_meters = 1.265
pulley_spacing_steps = pulley_spacing_meters * steps_per_meter
print(f"Pulley spacing in steps: {pulley_spacing_steps:.2f}")

Vx_sample = 0.0  # No movement in x direction
Vy_sample = 0.05*20308 # Movement in y direction
print("1015.40")

velA, velB = calculate_velocities(posA_sample, posB_sample, pulleySpacing_sample, Vx_sample, Vy_sample)
print(f"Velocity A: {velA:.6f}, Velocity B: {velB:.6f}")
