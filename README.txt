Zugfish Learning Upgrade v2

This archive contains only the source files changed for the requested learning/UI upgrade.
It does not include the Stockfish executable or compiled build output.

Apply it safely:
1. Close Zugfish.
2. Back up the current project folder (or at minimum the files contained in this archive).
3. Extract this ZIP into the root of your existing zugfish-main project folder.
4. Preserve folder structure and allow the seven listed source files to be replaced.
5. Open MSYS2 UCRT64 in the project root and run:
   cmake --build build
6. If the build succeeds, launch:
   ./build/gui/zugfish-qt.exe

Changes:
- Learning continuations now target 20 plies (10 moves per side), including the candidate first move.
- The Best continuation displays Best badges; the punishment line marks only the original played move as a mistake, with engine replies marked Best.
- The second line is named Opponent's punishment and begins with the move actually played, followed by the engine's strongest response and continuation.
- Each qualifying mistake has separate Best line and Punishment buttons and displays the recommended move.
- The learning section is moved higher in the review panel and highlighted.
- Workspace spacing is reduced to give the board more room and reduce the review panel width.

Note: This source patch has not been compiled in this environment. Compile on the same MSYS2 UCRT64 setup and share the complete output if any error appears.
