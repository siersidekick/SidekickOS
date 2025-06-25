# Contributing to OpenSidekick

We welcome contributions to the OpenSidekick project! This guide will help you get started.

## 🤝 **How to Contribute**

### **Types of Contributions**

We welcome all types of contributions:

- 🐛 **Bug Reports**: Found a bug? Let us know!
- 💡 **Feature Requests**: Have an idea? We'd love to hear it!
- 📝 **Documentation**: Help improve our docs
- 💻 **Code**: Submit bug fixes or new features
- 🧪 **Testing**: Help test new features and report issues
- 🎨 **Design**: UI/UX improvements for client applications
- 🔧 **Hardware**: 3D models, mounting solutions, hardware guides

### **Areas for Contribution**

**High Priority:**
- AI integration (ChatGPT, local models)
- Voice activation ("Hey Sidekick")
- Mobile app clients (React Native, Flutter)
- Hardware mounting solutions
- Performance optimizations

**Medium Priority:**
- Live streaming to social platforms
- Real-time object recognition
- Augmented reality overlays
- Additional client platforms

**Always Welcome:**
- Documentation improvements
- Bug fixes
- Code optimization
- Testing and validation

## 🚀 **Getting Started**

### **1. Fork and Clone**

```bash
# Fork the repository on GitHub
# Then clone your fork
git clone https://github.com/yourusername/opensidekick.git
cd opensidekick

# Add upstream remote
git remote add upstream https://github.com/originalowner/opensidekick.git
```

### **2. Set Up Development Environment**

**For Firmware Development:**
```bash
# Install ESP-IDF
git clone -b v5.0 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
source export.sh

# Build and test
cd opensidekick/firmware
idf.py build
```

**For Client Development:**
```bash
# Install Python dependencies
cd opensidekick/opensidekick-client
pip install -r requirements.txt
pip install -e .

# Run tests
python -m pytest
```

### **3. Create a Branch**

```bash
# Create a feature branch
git checkout -b feature/your-feature-name

# Or a bug fix branch
git checkout -b fix/issue-description
```

## 📋 **Development Guidelines**

### **Code Style**

**C/C++ (Firmware):**
- Follow ESP-IDF coding standards
- Use descriptive variable names
- Add comments for complex logic
- Keep functions focused and small
- Use proper error handling

**Python (Client):**
- Follow PEP 8 style guide
- Use type hints where appropriate
- Add docstrings to functions and classes
- Use meaningful variable names
- Include error handling

**HTML/JavaScript (Web Client):**
- Use modern JavaScript (ES6+)
- Follow consistent indentation (2 spaces)
- Add comments for complex logic
- Use semantic HTML
- Ensure mobile compatibility

### **Commit Messages**

Use clear, descriptive commit messages:

```
feat: add voice activation support
fix: resolve BLE connection timeout issue
docs: update hardware setup guide
refactor: optimize camera capture performance
test: add unit tests for audio encoding
```

**Format:**
- `feat:` New features
- `fix:` Bug fixes
- `docs:` Documentation changes
- `refactor:` Code refactoring
- `test:` Adding or updating tests
- `style:` Code style changes
- `perf:` Performance improvements

### **Testing**

**Before submitting:**
- Test your changes thoroughly
- Ensure existing functionality still works
- Add tests for new features
- Test on actual hardware when possible
- Verify documentation is accurate

**Testing Checklist:**
- [ ] Firmware builds successfully
- [ ] BLE connection works
- [ ] Camera capture functions
- [ ] Audio streaming works
- [ ] Client applications connect
- [ ] No memory leaks or crashes
- [ ] Performance is acceptable

## 🐛 **Reporting Issues**

### **Bug Reports**

When reporting bugs, include:

1. **Environment Information:**
   - Hardware: XIAO ESP32S3 Sense version
   - Firmware: Version and build date
   - Client: Python version, OS, browser
   - ESP-IDF: Version

2. **Steps to Reproduce:**
   - Clear, numbered steps
   - Expected behavior
   - Actual behavior
   - Screenshots/videos if helpful

3. **Logs and Output:**
   - Serial monitor output
   - Error messages
   - Debug information

**Template:**
```markdown
## Bug Report

**Environment:**
- Hardware: XIAO ESP32S3 Sense v1.0
- Firmware: v1.0.0 (Dec 28, 2024)
- Client: Python 3.9 on macOS 14.1
- ESP-IDF: v5.0

**Description:**
Brief description of the issue

**Steps to Reproduce:**
1. Step one
2. Step two
3. Step three

**Expected Behavior:**
What should happen

**Actual Behavior:**
What actually happens

**Logs:**
```
Paste relevant logs here
```

**Additional Context:**
Any other relevant information
```

### **Feature Requests**

When requesting features:

1. **Describe the feature clearly**
2. **Explain the use case**
3. **Consider implementation complexity**
4. **Suggest possible approaches**

## 🔄 **Pull Request Process**

### **Before Submitting**

1. **Update Documentation:**
   - Update README if needed
   - Add/update code comments
   - Update API documentation

2. **Test Thoroughly:**
   - Test on actual hardware
   - Verify existing functionality
   - Add automated tests if possible

3. **Check Code Quality:**
   - Follow coding standards
   - Remove debug code
   - Optimize performance

### **Pull Request Template**

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code refactoring

## Testing
- [ ] Tested on hardware
- [ ] Existing tests pass
- [ ] New tests added
- [ ] Documentation updated

## Screenshots/Videos
If applicable, add screenshots or videos

## Checklist
- [ ] Code follows project style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] No breaking changes (or clearly documented)
```

### **Review Process**

1. **Automated Checks:** CI/CD will run automated tests
2. **Code Review:** Maintainers will review your code
3. **Testing:** Changes will be tested on hardware
4. **Feedback:** Address any requested changes
5. **Merge:** Once approved, changes will be merged

## 🏷️ **Labels and Tags**

We use labels to categorize issues and PRs:

**Type:**
- `bug` - Bug reports
- `enhancement` - New features
- `documentation` - Documentation improvements
- `question` - Questions and discussions

**Priority:**
- `high-priority` - Critical issues
- `medium-priority` - Important improvements
- `low-priority` - Nice-to-have features

**Component:**
- `firmware` - ESP32S3 firmware
- `python-client` - Python client library
- `web-client` - Web browser client
- `hardware` - Hardware-related
- `documentation` - Documentation

**Status:**
- `good-first-issue` - Good for beginners
- `help-wanted` - Looking for contributors
- `in-progress` - Currently being worked on

## 🏆 **Recognition**

Contributors will be recognized in:
- README.md contributors section
- Release notes
- Project documentation
- Special contributor badges

## 📞 **Getting Help**

Need help contributing?

- **GitHub Discussions:** Ask questions and discuss ideas
- **Issues:** Report bugs and request features
- **Documentation:** Check existing docs first
- **Community:** Join our community discussions

## 📜 **Code of Conduct**

### **Our Pledge**

We pledge to make participation in our project a harassment-free experience for everyone, regardless of age, body size, disability, ethnicity, gender identity and expression, level of experience, nationality, personal appearance, race, religion, or sexual identity and orientation.

### **Our Standards**

**Positive behavior includes:**
- Using welcoming and inclusive language
- Being respectful of differing viewpoints
- Gracefully accepting constructive criticism
- Focusing on what is best for the community
- Showing empathy towards other community members

**Unacceptable behavior includes:**
- Harassment, discrimination, or derogatory comments
- Trolling, insulting, or personal attacks
- Public or private harassment
- Publishing others' private information without permission
- Other conduct which could reasonably be considered inappropriate

### **Enforcement**

Project maintainers are responsible for clarifying standards and will take appropriate action in response to unacceptable behavior.

## 🙏 **Thank You**

Thank you for contributing to OpenSidekick! Your contributions help make smart glasses technology accessible to everyone.

---

**Together, we're building the future of open-source smart glasses! 🤓✨** 