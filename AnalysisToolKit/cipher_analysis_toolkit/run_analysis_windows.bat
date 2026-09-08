@echo off
mkdir report 2>nul
python analyze_ciphertexts.py --input . --output report
pause
