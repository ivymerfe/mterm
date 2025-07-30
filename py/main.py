from user.app import App
from pathlib import Path


if __name__ == "__main__":
    app = App()
    app.main(icon_path=str(Path(__file__).parent / "icon.ico"))
