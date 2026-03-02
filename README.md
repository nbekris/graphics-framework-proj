# Project 1: Deferred Shading

This project implements a **Deferred Rendering pipeline** using C++ and OpenGL. Developed as part of the CS 562 course at DigiPen, the system transitions from a traditional forward rendering approach to a multi-pass architecture to efficiently handle large numbers of local light sources.

---

## Features

**High Light Count:** Supports up to 500 point lights in a scene without significant impact on performance.

**Multi-Pass Pipeline:** Efficiently separates geometry processing from lighting calculations.

**Dynamic Light Spiral:** A procedurally generated spiral of 128 (default) alternating red, green, and blue lights.

**Debug Quad View:** Includes a split-screen mode to visualize individual G-buffer components for debugging.

---

## Implementation Details

### The Pipeline

The renderer utilizes three distinct passes to reconstruct the final frame:

1. **G-Buffer Pass:** Scene data is written to three separate floating-point textures within a Framebuffer Object (FBO) using Multiple Render Targets (MRT).

**Stored Data:** Pixel position, normal buffer, albedo (diffuse), and specular buffer.

2. **Global Lighting Pass:** Executes lighting calculations in screen space using a full-screen quad and includes global ambient light.

3. **Local Lights Pass:** Performed for each point light using a light volume sphere. It uses the same BRDF calculations as the global pass but excludes ambient light and incorporates attenuation.

### Lighting Model

The project utilizes the **Blinn-Phong** algorithm, with calculations performed using a **GGX BRDF**.

**Conversion:** Shininess values are converted to roughness for use in the GGX distribution.

**Attenuation:** Light fall-off is calculated for each point light based on its radius and distance to the pixel.

---

## Limitations

* No support for shadow mapping.

* Image-Based Lighting (IBL) and reflections are not implemented.

* Current textures are mismatched.

**Optimization Note:** Currently stores light and eye vectors in textures, which is inefficient; these should ideally be calculated directly in the shader to save buffer space.


---

## What I Learned

* Abstracting OpenGL FBO functions into reusable methods and classes.


* Implementing and converting between different lighting models (Phong to GGX).


* Restructuring shader code to accommodate deferred shading logic.


* Creating a multi-pass rendering architecture.


---

## 🔗 References

* 
[LearnOpenGL: Deferred Shading](https://learnopengl.com/Advanced-Lighting/Deferred-Shading) 

**Author:** Niko Bekris 
