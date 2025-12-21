## What this repo is
A C++ / OpenGL black hole visualization project where I experimented with light bending and gravitational lensing around a (Schwarzschild-inspired) black hole + a single horizontal accretion disk.

This isn’t meant to be a perfect/validated physics simulator — it’s a learning project focused on:
- C++ graphics / rendering pipeline
- stepping rays through space (Newtonian → Schwarzschild-inspired)
- seeing how lensing can make one disk *look* like “two” from the observer’s view

## Notes / derivations
My derivations for the Newtonian and Schwarzschild numerical integration steps in polar coordinates are in the PDF in this repo:
- `Schwarzschild&Newton.pdf`

## Learning resources I used

### OpenGL
- https://youtube.com/playlist?list=PLn3eTxaOtL2PHxN8EHf-ktAcN-sGETKfw&si=C4GZEtVIFcEFxFJq

### Raymarching / SDFs
- Intro: https://youtu.be/khblXafu7iA?si=wGngvNMgPpH723Zi
- Tutorials / demos / SDF derivations (Inigo Quilez): https://iquilezles.org/

### Lagrangian mechanics (how I approached the geodesic stepping idea)
- How I first found this approach: https://youtu.be/jCD_4mqu4Os?si=rChoaYA6uNUe9rB2
- History & derivation: https://youtu.be/QbnkIdw0HJQ?si=i3TullrcJZamv8wM
- Derivation (another perspective): https://youtu.be/VCHFCXgYdvY?si=ORQCPyDTA9AfSyg_

## If you’re browsing the code
The core idea is: generate a camera ray → step it forward → check for BH/disk intersection → shade the hit.
Feel free to open issues if something’s confusing or you spot mistakes — I’m happy to improve the explanations.
