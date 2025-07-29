import base
import core
from .terminal import Terminal
from . import theme


class App(base.BaseApp):
    def create_terminal(self):
        existing_ids = {terminal.id for terminal in self.terminals}
        terminal_id = 0
        while terminal_id in existing_ids:
            terminal_id += 1
        terminal = Terminal(self, terminal_id)
        self.terminals.append(terminal)
        self.active_terminal = terminal
    
    def update_selection(self, x, y):
        updated = super().update_selection(x, y)
        hovered_button = -1
        if y > theme.Window.CAPTION_SIZE:
            if x <= self.get_selector_width():
                self.set_cursor(core.cursors.ARROW)
                button_idx = (y - theme.Window.CAPTION_SIZE) // theme.Selector.WIDTH
                if button_idx <= len(self.terminals):
                    hovered_button = button_idx
            else:
                self.set_cursor(core.cursors.IBEAM)

        if self.selector_hovered_button != hovered_button:
            self.selector_hovered_button = hovered_button
            updated = True

        return updated

    def on_mousemove(self, x, y):
        super().on_mousemove(x, y)
        if self.active_terminal is not None:
            self.active_terminal.on_mousemove(x, y)
    
    def on_mousedown(self, button, x, y):
        super().on_mousedown(button, x, y)
        if self.active_terminal is not None:
            self.active_terminal.on_mousedown(button, x, y)

    def on_mouseup(self, button, x, y):
        super().on_mouseup(button, x, y)
        updated = False
        if self.selector_hovered_button != -1:
            if button == core.buttons.LEFT:
                if self.selector_hovered_button < len(self.terminals):
                    terminal = self.terminals[self.selector_hovered_button]
                    if terminal != self.active_terminal:
                        self.active_terminal = terminal
                        updated = True
                else:
                    self.create_terminal()
                    updated = True
            elif button == core.buttons.MIDDLE:
                if self.selector_hovered_button < len(self.terminals):
                    self.destroy_terminal(self.terminals[self.selector_hovered_button])
                    updated = True
        
        if self.active_terminal is not None:
            self.active_terminal.on_mouseup(button, x, y)
        
        if updated:
            self.redraw()
    
    def on_doubleclick(self, button, x, y):
        super().on_doubleclick(button, x, y)
        if self.active_terminal is not None:
            self.active_terminal.on_doubleclick(button, x, y)
    
    def on_mouseleave(self):
        super().on_mouseleave()
        if self.active_terminal is not None:
            self.active_terminal.on_mouseleave()

    def on_scroll(self, delta, x, y):
        if self.active_terminal is not None:
            self.active_terminal.on_scroll(delta)

    def on_input(self, char):
        if self.active_terminal is not None:
            self.active_terminal.on_input(char)

    def on_keydown(self, key):
        if core.is_key_down(core.keys.LMENU):
            return
        if self.active_terminal is not None:
            self.active_terminal.on_keydown(key)
    
    def maybe_navigate(self, idx):
        if 0 <= idx < len(self.terminals):
            self.active_terminal = self.terminals[idx]
        else:
            self.create_terminal()
        self.redraw()

    def on_keyup(self, key):
        if core.is_key_down(core.keys.LMENU):
            if key == ord('Q'):
                self.destroy_terminal(self.active_terminal)
                self.redraw()
            elif key == core.keys.OEM_3: # grave accent key
                self.maybe_navigate(0)
            elif ord('1') <= key <= ord('9'):
                idx = key - ord('1') + 1
                self.maybe_navigate(idx)
            return
        if self.active_terminal is not None:
            self.active_terminal.on_keyup(key)
