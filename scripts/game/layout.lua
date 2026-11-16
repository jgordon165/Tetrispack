-- Where things sit in the 640x440 client area, as the original lays out a
-- maximized 640x480 window: the playfield at 0.325 x width, 0.066 x height,
-- 0.35 x width wide and 0.868 x height tall (TETRIS.EXE's WM_SIZE handler),
-- cells of width/10 by height/20; the score panel and its contents measured
-- from the original's screen.
local layout = { w = 640, h = 440 }

layout.grid = { x = 209, y = 29, cols = 10, rows = 20 }
layout.grid.cw = math.floor(0.35 * layout.w / 10)    -- 22
layout.grid.ch = math.floor(0.868 * layout.h / 20)   -- 19
layout.grid.margin = { 5, 5, 4, 4 }                   -- gray between the frame and the black (l, t, r, b)

layout.panel = { x = 17, y = 39, w = 91, h = 200 }
layout.labels = {
  { text = "Score:", x = 29, y = 63, vx = 37, vy = 80, field = "score" },
  { text = "Level:", x = 29, y = 97, vx = 37, vy = 114, field = "level" },
  { text = "Lines:", x = 29, y = 131, vx = 37, vy = 148, field = "lines" },
}
layout.next = { x = 29, y = 170, w = 70, h = 63, cw = 14, ch = 12 }

-- The black of the playfield: exactly the cells.
function layout.well()
  local g = layout.grid
  return g.x, g.y, g.cols * g.cw, g.rows * g.ch
end

return layout
