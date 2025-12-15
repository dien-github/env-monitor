#!/bin/bash

# Create a virtual environment and install dependencies
python3 -m venv venv

# Activate the virtual environment
source venv/bin/activate

# Install required Python packages
pip install -r requirements.txt