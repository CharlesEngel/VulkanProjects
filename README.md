# VulkanProjects
A couple of projects I made using Vulkan to practice graphics programming. One of them is a simple deferred renderer with physically based BRDFs for the lighting, the other is an implementation of ambient occlusion.

The basic structure is that there is a base class which implements the basic functionality needed to use vulkan quickly. Then the deferred and ambient occlusion engines inherent from the base class, and implement the actual algorithms needed to render. To help make startup time faster, I made an "asset packer" which puts pngs and .objs into files which are faster to read. Also, statically linking libraries made the compile time really slow, so I implemented functionality where pipelines, render passes, and materials can be modified in separate text files without recompiling.

To build, first clone the repository.

Then create a directory called "third party" and place the source code for the libraries glm, imgui, stb_image, tinyobjloader, vkbootstrap, and vma in that directory.

Finally, go to the build directory and run "make deferred" for the deferred rendering project or run "make ao" for the ambient occlusion project.

Output file will be named "app"

Models and textures were taken from online and are not my own.
