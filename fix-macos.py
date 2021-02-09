from pathlib import Path


def main():
    old_path = Path("env/bin/python")
    # resolve the symlinks, should be "*/Frameworks/Python.framework/Versions/3.9/bin/python3.x"
    path = old_path.resolve(strict=True)
    print("Resolved path", path)
    path = (path.parent.parent / "Resources" / "Python.app" / "Contents" / "MacOS" / "Python")
    print("Replacement path", path)
    # make sure the replacement exists
    new_path = path.resolve(strict=True)
    old_path.rename("env/bin/old")
    old_path.symlink_to(new_path)
    print("Done")


if __name__ == "__main__":
    main()
