# Contributing to Infra

Thank you for your interest in contributing to the Infra project!

## Development Workflow

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Commit your changes (`git commit -m 'Add amazing feature'`)
7. Push to the branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

## Coding Standards

### Style Guidelines

- Follow existing code style
- Use meaningful variable names
- Limit line length to 80 characters when possible
- Use spaces for indentation (4 spaces per level)

### Documentation

- Document all public APIs in the header files
- Update README.md for new features
- Add comments for complex algorithms
- Include usage examples

### Testing

- Write tests for new functionality
- Ensure all tests pass before submitting
- Include error condition testing
- Performance benchmarking for critical paths

## File Organization

```
infra/
├── memory/          # Memory allocation components
│   ├── slab.h      # Main slab allocator header
│   └── slab_test.cc # Test file
├── docs/           # Documentation
│   └── API.md      # API documentation
├── tests/          # Additional tests (future)
├── examples/       # Usage examples (future)
├── README.md       # Project overview
├── Makefile        # Build configuration
└── CONTRIBUTING.md  # Contribution guidelines
```

## Pull Request Process

1. **Title**: Use a clear and descriptive title
2. **Description**: Explain what your PR does and why
3. **Related Issues**: Link to any relevant issues
4. **Testing**: Describe what you tested
5. **Breaking Changes**: Clearly mark any breaking changes

## Reporting Issues

When reporting bugs, please include:

- Environment details (OS, compiler version)
- Steps to reproduce the issue
- Expected vs actual behavior
- Minimal example code if applicable
- Error messages (if any)

## License

By contributing to this project, you agree that your contributions will be licensed under the MIT License.