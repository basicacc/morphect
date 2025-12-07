# Contributing

Thanks for wanting to help out!

## Getting started

1. Fork and clone the repo
2. Build it:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build -j$(nproc)
   ```
3. Run tests to make sure everything works:
   ```bash
   ctest --test-dir build
   ```

## Code style

Nothing too fancy:
- 4 space indent (no tabs)
- `snake_case` for variables and functions
- `PascalCase` for classes
- Keep lines under 100 chars when reasonable
- Write code that explains itself, don't comment obvious stuff

## Adding new passes

If you want to add a new obfuscation technique:

1. Create a header in `src/passes/<category>/your_pass.hpp`
2. Inherit from `LLVMTransformationPass`, `GimpleTransformationPass`, or `AssemblyTransformationPass`
3. Implement the required methods:
   - `getName()`
   - `getPriority()`
   - `transform*()` (the actual transformation)
4. Add tests in `tests/unit/`
5. Register it in the pass manager

Example skeleton:

```cpp
class MyPass : public LLVMTransformationPass {
public:
    std::string getName() const override { return "MyPass"; }
    PassPriority getPriority() const override { return PassPriority::Data; }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        // do stuff
        return TransformResult::Success;
    }
};
```

## Tests

- Add tests for new features
- Don't break existing tests
- Run the full suite before submitting

## Pull requests

- Keep PRs focused - one feature or fix per PR
- Write a decent description of what you changed and why
- Make sure CI passes

## Questions?

Open an issue if something's unclear.
