require "include/protoplug"

-- # GLOBALS

-- ## Params exposed to the host

local firstDegreeCode = 60
local chordChan = 16

local noChordNoteBehVals = {"Latch last chord"; "Silence"; "Use pattern as notes"}
local noChordNoteBeh = noChordNoteBehVals[1];

local singleChordNoteBehVals = {"Transpose last chord"; "Powerchord"; "Use as is"; "Silence"; "Use pattern as notes"}
local singleChordNoteBeh = singleChordNoteBehVals[1];

local chordNotesPassthrough = false;

local ignoreBlackKeysInPatterns = false;

-- ## Private global vars

local counters = {}
local curMappings = {}

for chan = 1,16 do
    curMappings[chan] = {}
end

local lastChord = nil


-- # UTILITY FUNCTIONS

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

function isBlackKey(code)
    local rem = code % 12
    return rem == 1 or rem == 3 or rem == 6 or rem == 8 or rem == 10
end

function getDegreeShift(code)
    local x = code - firstDegreeCode
    if ignoreBlackKeysInPatterns then
        -- We need to correct the [firstDegreeCode,code] interval for the amount of black keys it contains:
        local sign = (x < 0) and -1 or 1
        local absX = sign * x
        for i = firstDegreeCode,code,sign do
            if isBlackKey(i) then
                absX = absX - 1
            end
        end
        return sign*absX
    else
        return x
    end
end

function transformEvent(curChord, ev)
  if ev:getNote() then
    local chan = ev:getChannel()
    local noteCodeIn = ev:getNote()
    local degShift = getDegreeShift(noteCodeIn)
    if ev:isNoteOn() then
      local wantedDegree = degShift % #curChord + 1
      local wantedOctaveShift = math.floor(degShift / #curChord)
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
end

function onlyNoteOffs(events)
    newEvents = {}
    for _,ev in ipairs(events) do
        if ev:isNoteOff() then
            table.insert(newEvents, ev)
        end
    end
    return newEvents
end

-- # MAIN FUNCTION

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
        else -- Event on some pattern chan:
            c = ev:getNote()
            if not (c and ignoreBlackKeysInPatterns and isBlackKey(c)) then
                table.insert(eventsToProcess, midi.Event(ev))
            end
        end
    end
    midiBuf:clear()
    
    -- Depending on settings, determine the current wanted chord:
    local curChord = getCurChord()
    local doProcess = true
    if #curChord >= 2 then
        -- Chord contains at least 2 notes. This is the "normal" situation.
        -- We register the current chord if we need it later
        lastChord = curChord
    elseif #curChord == 1 then
        -- Chord contains only one note. Behaviour depends on "When single chord note" parameter
        if singleChordNoteBeh == singleChordNoteBehVals[1] and lastChord then -- Transpose last chord (if we have one)
            local offset = curChord[1] - lastChord[1]
            curChord = {}
            for _,code in ipairs(lastChord) do
                table.insert(curChord, code+offset)
            end
        elseif singleChordNoteBeh == singleChordNoteBehVals[2] then -- Powerchord
            curChord[2] = curChord[1] + 7
        elseif singleChordNoteBeh == singleChordNoteBehVals[3] then -- Use "one-note chord" as it is
            -- Do nothing
        elseif singleChordNoteBeh == singleChordNoteBehVals[5] then -- Use pattern as notes (Ignore one-note chord)
            doProcess = false
        else -- Silence
            eventsToProcess = onlyNoteOffs(eventsToProcess)
        end
    else -- Chord is empty. Behaviour depends on "When no chord note" parameter
        if noChordNoteBeh == noChordNoteBehVals[1] and lastChord then -- Use last chord (if we have one)
            curChord = lastChord
        elseif noChordNoteBeh == noChordNoteBehVals[3] then -- Use pattern as notes
            doProcess = false
        else -- Silence
            eventsToProcess = onlyNoteOffs(eventsToProcess)
        end
    end
    
    -- Pass non-processable events through:
    for _,ev in pairs(otherEvents) do
        midiBuf:addEvent(midi.Event(ev))
    end
    
    -- Process and add processable events:
    for _,ev in pairs(eventsToProcess) do
        if doProcess then
            transformEvent(curChord, ev)
        end
        midiBuf:addEvent(ev)
    end
end


-- # EXPOSED PARAMETERS

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
    { name = "Chord notes passthrough";
      type = "list";
      values = {false; true};
      default = false;
      changed = function(x) chordNotesPassthrough = x end
    };
    { name = "When no chord note";
      type = "list";
      values = noChordNoteBehVals;
      default = noChordNoteBehVals[1];
      changed = function(x) noChordNoteBeh = x end
    };
    { name = "When single chord note";
      type = "list";
      values = singleChordNoteBehVals;
      default = singleChordNoteBehVals[1];
      changed = function(x) singleChordNoteBeh = x end;
    };
    { name = "Ignore black keys in patterns";
      type = "list";
      values = {false; true};
      default = ignoreBlackKeysInPatterns;
      changed = function(x) ignoreBlackKeysInPatterns = x end
    };
}
