/* Apache-2.0. The About screen's website row. */
#pragma once
/* Opens a page in the default browser: NSWorkspace in the AppKit Mac app, UIApplication elsewhere (the Apple TV has no
   browser and does nothing). Page 0 is https://visr.gg/, 1 its privacy page and 2 the license in the repository. */
void host_open_website(int page);
