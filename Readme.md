# atomic

A single-pass, hardware-accelerated vector and typography canvas built from scratch on Vulkan. No web-tech bloat, no massive external dependencies—just raw pixel layout speed running straight on the GPU.

Right now, it is in its early "move fast, break things, and stare at blank validation screens" phase. 

-------------------------------------------------------------------------------
  The Blueprint
-------------------------------------------------------------------------------

Most UI toolkits slow down because they separate panels, borders, and text into different passes. Atomic smashes all of that into a single, unified instance stream.

  [ Your UI Code ] ---> UIInstance Queue ---> Single Draw Call ---> GPU Canvas

Every rectangle, rounded panel, circle, and text glyph maps directly to an irreducible atomic unit, letting a single custom fragment shader evaluate shapes, borders, and mathematical curves instantly.

-------------------------------------------------------------------------------
  What Works Right Now
-------------------------------------------------------------------------------

* Unified Batching: Primitive shapes and raw text layout execute in a single pass.
* Vector Control: Independent control over individual corner radii, stroke positions, and custom dashed lines.
* Live Type Processing: FreeType backend that bakes glyph metrics and packs them on the fly into a single-channel GPU atlas.
* Zero-Bleed State: Strict data packing ensures your panels never accidentally inherit font styles or garbage stack memory.

-------------------------------------------------------------------------------
  The Roadmap
-------------------------------------------------------------------------------

[-] Linear & Radial Gradients: Multi-stop color interpolation calculated directly in the shader.
[-] Image Fills & Textures: Global descriptor array indexing to map images into any primitive shape.
[-] Block Text Gradients: Spanning fluid color blends smoothly across entire words instead of single character boxes.
[-] Custom Polygons: Moving past hardcoded quad generation to support arbitrary vector paths and graphs.
[-] SVG Integration: Lightweight parsing pipelines to drop vector files directly into the live layout.

-------------------------------------------------------------------------------
  Contributing & Chaos
-------------------------------------------------------------------------------

Breaking changes are the default state of affairs here. If you are down to fight with Vulkan synchronization barriers, optimize memory layouts, and build a UI engine from the metal up, pull requests are welcome.
