# Contributing to jsocketpp

<!--! [TOC] -->

Thank you for your interest in contributing to **jsocketpp**!
We welcome issues, discussions, and pull requests. Please follow the guidelines below.

---

## 1. Reporting Issues

- **Bugs:** Please search [existing issues](https://github.com/MangaD/jsocketpp/issues) before opening a new one.
- Include OS/compiler details, clear reproduction steps, expected/actual results, and code samples if possible.
- **Feature Requests:** Clearly describe your proposal and motivation.

---

## 2. Submitting Pull Requests

- Fork, clone, and branch from `main`:
  ```sh
  git checkout -b feature/my-feature
  ```

* Make your changes.
* **Format your code with the `.clang-format` file in the project root**:
    * Use: `clang-format -i <your-changed-files>`
    * Or configure your IDE (e.g., CLion) to auto-format using this file.
* Add or update unit tests if relevant.
* Commit with a descriptive message.
* Push and open a Pull Request (PR) against `main`.

---

## 3. Code Style Guidelines

* **Code style is enforced by the provided `.clang-format`** (LLVM-based, Allman braces, 4 spaces, left-aligned
  pointers, etc.).
* **CLion users:** You should see live suggestions to match the style. Please accept these suggestions or run "Reformat
  Code" before committing.
* **Naming conventions:**

    * **Private members:** Use `lowerCamelCase` and prefix with an underscore (e.g., `_sockFd`, `_buffer`,
      `_localAddr`).
    * **Types:** `CamelCase` (e.g., `DatagramSocket`).
    * **Methods:** `lowerCamelCase` (e.g., `setNonBlocking()`).
* Use modern C++17 features where appropriate.
* Write cross-platform code (`#ifdef _WIN32` etc.) where required.
* Document **all public classes and methods** with Doxygen-style comments.

---

## 4. Pre-commit Hooks

* We recommend using [pre-commit](https://pre-commit.com/) to automatically check formatting and lint code before each
  commit.
* To set up pre-commit hooks:
    1. Install Python dependencies (`pre-commit` and its dependencies):
    ```sh
    pip install -r requirements.txt
    ```

    2. Install the hooks defined in `.pre-commit-config.yaml`. Every time you clone a project using pre-commit running
       `pre-commit install` should always be the first thing you do.
    ```sh
    pre-commit install
    ```
    3. (Recommended) Periodically update the hooks to their latest versions:
    ```sh
    pre-commit autoupdate
    ```
    4. Now, hooks will run automatically on `git commit`. You can manually run them on all files with:
    ```sh
    pre-commit run --all-files
    ```
  The first time pre-commit runs on a file it will automatically download, install, and run the hook. Note that
  running a hook for the first time may be slow. For example: If the machine does not have node installed,
  pre-commit will download and build a copy of node.
* Please ensure all hooks pass before pushing your changes.

---

## 5. Testing

* All PRs must pass existing tests and should include new unit tests for new features or bugfixes.
* We use [GoogleTest](https://github.com/google/googletest).
* Build and run tests:

  ```sh
  cmake --preset=debug
  cmake --build --preset=debug
  ctest --preset=debug
  ```

---

## 6. Documentation

* Update [README.md](README.md) and API comments for any new features or changes.
* Public APIs require Doxygen documentation.

---

## 7. Branching & Code of Conduct

* Base your PRs on `main`.
* Use feature (`feature/xyz`) or bugfix (`bugfix/issue123`) branch names.
* Follow our [Code of Conduct](CODE_OF_CONDUCT.md).

---

## 8. Questions

Open an [issue](https://github.com/MangaD/jsocketpp/issues) or join the discussion tab.

Thank you for your contributions!
