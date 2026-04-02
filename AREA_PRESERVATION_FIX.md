# Area Preservation Bug Fixes

## Problem Summary
Your code was not preserving area exactly because the `displacementArea()` function was using **absolute values** on intermediate triangle area calculations, destroying the **signed area** property required by the Kronenfeld et al. area-preserving algorithm.

## Root Causes

### Issue 1: `triArea()` Function
**Original code:**
```cpp
inline double triArea(Point i, Point j, Point k)
{
    return (j.x-i.x) * (k.y-i.y) - (k.x-i.x)*(j.y-i.y);
}
```

**Problem:** While this computes a signed area (cross product), it's not the standard shoelace formula. It's equivalent but can be clearer.

**Fixed code:**
```cpp
inline double triArea(Point i, Point j, Point k)
{
    return i.x * (j.y - k.y) + j.x * (k.y - i.y) + k.x * (i.y - j.y);
}
```

**Why:** This is the standard shoelace formula for signed area. Both formulas are mathematically equivalent, but the standard form is clearer and less error-prone.

---

### Issue 2: `displacementArea()` Function (CRITICAL)
**Original code:**
```cpp
double displacementArea(Point A, Point B, Point C, Point D, Point E)
{
    // area removed
    double removed = std::abs(triArea(A, B, C)) + std::abs(triArea(A, C, D));
    
    // area added
    double added = std::abs(triArea(A, E, D));
    
    return std::abs(removed - added);
}
```

**Problem:** Taking `std::abs()` on intermediate triangle areas destroys the **signed area property**. According to the Kronenfeld paper:

- The original quadrilateral ABCD has signed area: `Area(A,B,C) + Area(A,C,D)`
- The simplified triangle AED has signed area: `Area(A,E,D)`
- When E is placed on the **area-preserving line**, these must be **exactly equal** (using signed areas)
- Therefore, displacement = `|original - simplified|` should be ≈ 0

By using `std::abs()` on each component, you're computing something else entirely that doesn't give you this property.

**Fixed code:**
```cpp
double displacementArea(Point A, Point B, Point C, Point D, Point E)
{
    // Original area using signed areas (preserves algebraic properties)
    double original = triArea(A, B, C) + triArea(A, C, D);
    
    // New area
    double simplified = triArea(A, E, D);
    
    // Displacement is the absolute difference
    // If E is on the preservation line, this should be ~0 (within floating point precision)
    return std::abs(original - simplified);
}
```

**Why:** 
1. Uses **signed areas** to preserve the mathematical property
2. When E is correctly placed on the preservation line, `original ≈ simplified`
3. Therefore `std::abs(original - simplified) ≈ 0` for minimal displacement
4. The total area of all collapses sums to essentially zero, preserving the total area

---

## Mathematical Principle from Kronenfeld et al. (Page 8-10)

**Area Preservation Condition (Equation 1a):**
$$\text{Area}^\Delta(\triangle ACB) + \text{Area}^\Delta(\triangle ADC) = A^\Delta(\triangle ADE)$$

Where:
- $\text{Area}^\Delta$ denotes **signed area** (not absolute value)
- $\triangle$ denotes a triangle
- When this condition holds, the point E lies on a specific line (the "E-line")

**Displacement:** 
$$\text{Displacement} = |\text{removed} - \text{added}|$$

Where removed and added use **signed areas**, not absolute values.

---

## Expected Behavior After Fix

1. **Input area = Output area** (exactly, within floating point precision)
2. **Minimal areal displacement** - each collapse has displacement ≈ 0 if E is on preservation line
3. The algorithm will minimize total displacement while preserving exact area

## Testing

To verify the fix works correctly:

1. Run the simplification on a test case
2. Check `output.txt` for:
   - "Total signed area in input" ≈ "Total signed area in output" (should be identical or very close)
   - "Total areal displacement" should be minimal

Example from the paper: On 10 lakes simplified to different scales, APSC with topology check had:
- **Area change: 0%** (perfect preservation)
- **Max displacement: 4.4 km²** at 5M scale (best among tested algorithms)
