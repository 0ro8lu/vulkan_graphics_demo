# Vulkan Graphics Demo

This is my personal repository for everything Vulkan related. In the next months i'm going to cook up a demo with all the graphics
knowledge i'll have accumulated thanks to the books and resources. Hope it is going to be something cool.

##18th December: Model loading
Oh boy what an endevoir this has been! Finally exiting the "Vulkan tutorial's comfort zone" and starting to head into sort of "unexplored" territory (at least for me).
I finally got the chance to get my hands dirty with multiple descriptor sets and really had to sit there thinking what could be the best possible strategy to upload
my precious data to the shaders. Currently i have been unable to get bindless texture working (i've tried, thanks MoltenVK!) due to "Macbeingapaininthearsetodevelopfor"-itis.
It is a rare and uncurable condition i'm afraid!
I've still managed to get it to work the ol' fashioned way by just simply allocating a descriptor set for each submesh.
Model matrix is uploaded as a push constant while view, projection and view direction are uploaded from another descriptor set, given their global status.
The code base is not in its best shape right now. Nothing is efficient (yet) as many resources are just reallocated without the need to, i just had to get something out to
start building a demo for my Rare internship cover letter.
Next days are going to be busy trying to build something cool, so no more progress until the end of December.

Next item on the menu will be: Resource cache, skyboxes, hopefully support for animation and bindless textures if they don't turn out to be a massive pain for a small gain.
Till next time :3 

