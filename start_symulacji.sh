#!/bin/bash

SESSION="symulacja_dyskontu"

if ! command -v tmux &> /dev/null; then
    echo "Błąd: Tmux nie jest zainstalowany."
    echo "Zainstaluj go komendą: sudo apt install tmux"
    exit 1
fi

tmux kill-session -t $SESSION 2>/dev/null

tmux new-session -d -s $SESSION

tmux set -g mouse on

tmux split-window -v

tmux select-pane -t 0
tmux split-window -h

tmux select-pane -t 2
tmux split-window -h

# Panel 0 (Lewa Góra)  | Panel 1 (Prawa Góra)
# -------------------------------------------
# Panel 2 (Lewy Dół)   | Panel 3 (Prawy Dół)

tmux select-pane -t 0
tmux send-keys "./kierownik" C-m

tmux select-pane -t 1
#tmux send-keys "echo 'Oczekiwanie na inicjalizację Kierownika...'; sleep 4; ./generator" C-m

tmux select-pane -t 2
tmux send-keys "watch -n 1 'ipcs -s && ipcs -m'" C-m

tmux attach-session -t $SESSION > /dev/null 2>&1

clear