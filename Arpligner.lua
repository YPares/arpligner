require "include/protoplug"

-- When no notes are playing

local defaultChord = {60,64,67,70} -- a C7 chord (at octave 3). You can edit that

-- Params exposed to the host:

local firstDegreeCode = 60
local chordChan = 16

local noChordBehVals = {"Use default chord (see script)"; "Silence"; "Passthrough"}
local noChordBeh = noChordBehVals[1];

local chordNotesPassthrough = false;

-- Private global vars:

local counters = {}
local curMappings = {}

for chan = 1,16 do
    curMappings[chan] = {}
end


-- We need to keep a counter for each current chord note, because for a given note,
-- note offs can happen after the next note on (we don't want one single note off
-- to cancel two consecutive note ons)
function addCurNote(note)
    counters[note] = (counters[note] or 0) + 1
end

function rmCurNote(note)
    x = (counters[note] or 1) - 1
    if x <= 0 then
        x = nil
    end
    counters[note] = x
end

function getCurChord()
    local chord = {}
    for note,c in pairs(counters) do
        if c > 0 then
            table.insert(chord, note)
        end
    end
    table.sort(chord)
    return chord
end


function transformEvent(curChord, ev)
  if ev:getNote() then
    local chan = ev:getChannel()
    local noteCodeIn = ev:getNote()
    if ev:isNoteOn() then
      if #curChord == 0 then
        if noChordBeh == noChordBehVals[2] then -- Silence
          return nil
        elseif noChordBeh == noChordBehVals[3] then -- Passthrough
          return ev
        else -- Use default chord
          curChord = defaultChord
        end
      end
      local wantedDegree = (noteCodeIn - firstDegreeCode) % #curChord + 1
      local wantedOctaveShift = math.floor((noteCodeIn - firstDegreeCode) / #curChord)
      local finalNote = curChord[wantedDegree] + 12*wantedOctaveShift
      local oct = wantedOctaveShift>0
              and "+"..wantedOctaveShift
              or (wantedOctaveShift == 0 and "--" or wantedOctaveShift)
      print("cur:"..table.concat(curChord, ","), "chan:"..chan, "deg:"..wantedDegree,
            "oct:"..oct, "final:"..finalNote)
      curMappings[chan][noteCodeIn] = finalNote
      ev:setNote(finalNote)
    elseif ev:isNoteOff() then
      local m = curMappings[chan][noteCodeIn]
      if m then
          ev:setNote(m)
      end
    end
  end
  return ev
end


function plugin.processBlock(samples, smax, midiBuf)
    local eventsToProcess = {}
    local otherEvents = {}
    for ev in midiBuf:eachEvent() do
        if ev:getChannel() == chordChan then
          if ev:isNoteOn() then
            addCurNote(ev:getNote())
          elseif ev:isNoteOff() then
            rmCurNote(ev:getNote())
          end
          if chordNotesPassthrough or (not ev:getNote()) then
            table.insert(otherEvents, midi.Event(ev))
          end
        else
            table.insert(eventsToProcess, midi.Event(ev))
        end
    end
    midiBuf:clear()
    
    local curChord = getCurChord()
    
    for _,ev in pairs(eventsToProcess) do
        ev2 = transformEvent(curChord, ev)
        if ev2 then
            midiBuf:addEvent(ev2)
        end
    end
    for _,ev in pairs(otherEvents) do
        midiBuf:addEvent(midi.Event(ev))
    end
end

plugin.manageParams {
    { name = "Chord channel";
      type = "int";
      min = 1;
      max = 16;
      default = 16;
      changed = function(x) chordChan = x end
    };
    { name = "First degree MIDI code";
      type = "int";
      min = 0;
      max = 127;
      default = 60;
      changed = function(x) firstDegreeCode = x end
    };
    { name = "When no chord note";
      type = "list";
      values = noChordBehVals;
      default = noChordBehVals[1];
      changed = function(x) noChordBeh = x end
    };
    { name = "Chord notes passthrough";
      type = "list";
      values = {false; true};
      default = false;
      changed = function(x) chordNotesPassthrough = x end
    };
}
