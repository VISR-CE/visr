# License texts

`CC0-1.0.txt` preserves the origin projects' public domain dedication.
The downloaded components' license snapshots let `tools/notices_text.py`
generate the combined notices without downloading or building their code:

- `SDL.txt`: SDL 3.4.16's `LICENSE.txt`.
- `musl.txt`: musl 1.2.5's complete `COPYRIGHT`, including component notices.
- `Khronos.txt`: the copyright notices and MIT and Apache-2.0 terms for the
  headers in `tools/ios_build.py`'s `HEADERS`, from OpenGL-Registry revision
  `1cdd228e34966dd6b95bd203e9f84faba0f371a1` and EGL-Registry revision
  `db3425b8246136faccb5e2782b5694960bd6edf1`.

Update these snapshots when updating their dependencies. After changing
`tools/about_data.py` or any license text, run `python3 tools/notices_text.py`
to regenerate `port/ios/Licenses/NOTICES.txt`. Run
`python3 tools/notices_text.py --check` to check that it is current.
