require "include/protoplug"

local firstDegreeCode = 60


local counters = {}
local chordChan = 16
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


function transformEvent(curChord, ev0)
  local ev = midi.Event(ev0)
  if ev:getNote() then
    local finalNote = nil
    local chan = ev:getChannel()
    local noteCodeIn = ev:getNote()
    if ev:isNoteOn() and #curChord > 0 then
        local wantedDegree = (noteCodeIn - firstDegreeCode) % #curChord + 1
        local wantedOctaveShift = math.floor((noteCodeIn - firstDegreeCode) / #curChord)
        finalNote = curChord[wantedDegree] + 12*wantedOctaveShift
        local oct = wantedOctaveShift>0
                and "+"..wantedOctaveShift
                or (wantedOctaveShift == 0 and "--" or wantedOctaveShift)
        print("cur:"..table.concat(curChord, ","), "chan:"..chan, "deg:"..wantedDegree, "oct:"..oct, "final:"..finalNote)
        ev:setNote(finalNote)
    elseif ev:isNoteOff() then
        local m = curMappings[chan][noteCodeIn]
        if m then
            ev:setNote(m)
        end 
    end
    curMappings[chan][noteCodeIn] = finalNote
  end
  return ev
end


function plugin.processBlock(samples, smax, midiBuf)
    local otherEvs = {}
    for ev in midiBuf:eachEvent() do
        if ev:getChannel() == chordChan then
            if ev:isNoteOn() then
                addCurNote(ev:getNote())
            elseif ev:isNoteOff() then
                rmCurNote(ev:getNote())
            else
                table.insert(otherEvs, ev)
            end
        else
            table.insert(otherEvs, ev)
        end
    end
    
    local curChord = getCurChord()
    
    midiBuf:clear()
    for _,ev in pairs(otherEvs) do
        midiBuf:addEvent(transformEvent(curChord, ev))
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
    }
}