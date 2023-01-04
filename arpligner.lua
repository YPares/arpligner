require "include/protoplug"

local counters = {}
local middleC = 60
local chordChan = 16
local curMappings = {}

for chan = 1,chordChan-1 do
    curMappings[chan] = {}
end

-- We need to keep a counter for each current chord note, because for a given note,
-- note offs can happen after the next note on (we don't want one single note off
-- to cancel two consecutive note ons)
function addCurNote(note)
    counters[note] = (counters[note] or 0) + 1
end

function rmCurNote(note)
    counters[note] = (counters[note] or 1) - 1
    if counters[note] <= 0 then
        counters[note] = nil
    end
end

function isInCurNotes(note)
    return (counters[note] or 0) > 0
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
        local wantedDegree = (noteCodeIn - middleC)%#curChord + 1
        local wantedOctaveShift = math.floor((noteCodeIn - middleC) / #curChord)
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
                -- TODO: sort new notes in the same buffer per pitch
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
    --print(table.concat(curChord,","))
    
    midiBuf:clear()
    for _,ev in pairs(otherEvs) do
        midiBuf:addEvent(transformEvent(curChord, ev))
    end
end