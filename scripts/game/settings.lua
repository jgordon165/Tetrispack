-- The options, kept between runs: tp.storage_write("settings"), one
-- "key=value" line each. Hosts without storage (or a first run) just get
-- the defaults.
local settings = {}

function settings.serialize(t)
  local keys = {}
  for k in pairs(t) do keys[#keys + 1] = k end
  table.sort(keys)
  local out = {}
  for _, k in ipairs(keys) do
    local v = t[k]
    if type(v) == "boolean" then v = v and "true" or "false" end
    out[#out + 1] = k .. "=" .. tostring(v):gsub("[%c]", " ")
  end
  return table.concat(out, "\n") .. "\n"
end

-- Values come back as strings, numbers or booleans.
function settings.parse(text)
  local t = {}
  for line in (text or ""):gmatch("[^\n]+") do
    local k, v = line:match("^([%w_]+)=(.*)$")
    if k then
      v = v:gsub("\r$", "")
      if v == "true" then t[k] = true
      elseif v == "false" then t[k] = false
      elseif tonumber(v) and v:match("^%-?%d+%.?%d*$") then t[k] = tonumber(v)
      else t[k] = v end
    end
  end
  return t
end

function settings.load()
  if tp and tp.storage_read then return settings.parse(tp.storage_read("settings")) end
  return {}
end

function settings.save(t)
  if tp and tp.storage_write then tp.storage_write("settings", settings.serialize(t)) end
end

return settings
