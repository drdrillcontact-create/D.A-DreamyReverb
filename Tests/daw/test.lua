-- Temporary debugging aid: loads the plugin in REAPER, lists its parameters, opens the
-- editor and renders audio through it. Results go to the file named by DAWTEST_OUT.
local outPath = os.getenv("DAWTEST_OUT") or "dawtest-result.txt"
local dir     = os.getenv("DAWTEST_DIR") or "."
local fxNames = os.getenv("DAWTEST_FX") or "VST3:D.A Dreamy Shimmer Reverb"

local f = io.open(outPath, "w")
local function log(...)
  local t = {}
  for i, v in ipairs({...}) do t[i] = tostring(v) end
  f:write(table.concat(t, " "), "\n"); f:flush()
end

log("REAPER", reaper.GetAppVersion(), reaper.GetOS())

-- 3 s stereo wav: 0.3 s of noise, then silence
local sr = 44100
local wavPath = dir .. "/dawtest-burst.wav"
do
  local frames = sr * 3
  local data = {}
  math.randomseed(1)
  for i = 1, frames do
    local v = 0
    if i <= sr * 0.3 then v = math.floor((math.random() - 0.5) * 20000) end
    data[i] = string.pack("<i2i2", v, v)
  end
  local body = table.concat(data)
  local w = io.open(wavPath, "wb")
  w:write("RIFF", string.pack("<I4", 36 + #body), "WAVE", "fmt ",
          string.pack("<I4I2I2I4I4I2I2", 16, 1, 2, sr, sr * 4, 4, 16),
          "data", string.pack("<I4", #body), body)
  w:close()
end

local function tailRms(item)
  local take = reaper.GetActiveTake(item)
  local acc = reaper.CreateTakeAudioAccessor(take)
  local n = sr
  local buf = reaper.new_array(n * 2)
  reaper.GetAudioAccessorSamples(acc, sr, 2, 1.0, n, buf)
  reaper.DestroyAudioAccessor(acc)
  local sum, peak, bad = 0, 0, 0
  for i = 1, n * 2 do
    local v = buf[i]
    if v ~= v then bad = bad + 1 else sum = sum + v * v; if math.abs(v) > peak then peak = math.abs(v) end end
  end
  return string.format("rms=%.6f peak=%.6f nan=%d", math.sqrt(sum / (n * 2)), peak, bad)
end

local trackIndex = 0
for name in string.gmatch(fxNames, "[^;]+") do
  reaper.InsertTrackAtIndex(trackIndex, false)
  local tr = reaper.GetTrack(0, trackIndex)
  local idx = reaper.TrackFX_AddByName(tr, name, false, -1)
  log("ADD [" .. name .. "] ->", idx)

  if idx >= 0 then
    local _, fxname = reaper.TrackFX_GetFXName(tr, idx, "")
    local n = reaper.TrackFX_GetNumParams(tr, idx)
    log("LOADED [" .. fxname .. "] params", n)
    for p = 0, n - 1 do
      local _, pn = reaper.TrackFX_GetParamName(tr, idx, p, "")
      local _, fv = reaper.TrackFX_GetFormattedParamValue(tr, idx, p, "")
      log("   param", p, pn, "=", fv)
    end

    reaper.TrackFX_Show(tr, idx, 3)

    reaper.SetOnlyTrackSelected(tr)
    reaper.SetEditCurPos(0, false, false)
    reaper.InsertMedia(wavPath, 0)
    local item = reaper.GetTrackMediaItem(tr, 0)
    if item then
      log("TAIL before FX:", tailRms(item))
      reaper.SelectAllMediaItems(0, false)
      reaper.SetMediaItemSelected(item, true)
      reaper.Main_OnCommand(40209, 0)   -- Item: Apply track/take FX to items
      log("TAIL after FX: ", tailRms(item))
    else
      log("no media item")
    end
  else
    log("FAILED to add [" .. name .. "]")
  end
  trackIndex = trackIndex + 1
end

log("DONE")

-- keep reporting for a while: the editor paints after the script returns
local start = reaper.time_precise()
local last = 0
local function alive()
  local t = reaper.time_precise() - start
  if t - last >= 2 then last = t; log("ALIVE", string.format("%.0f", t)) end
  if t < 13 then reaper.defer(alive) else log("END") end
end
reaper.defer(alive)
