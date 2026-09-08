% Run this once in MATLAB before calling the encryption script.
% It adds the current folder to the MATLAB path so helper functions are found.
thisDir = fileparts(mfilename('fullpath'));
if ~isempty(thisDir)
    addpath(thisDir);
end
fprintf('MATLAB path updated with: %s\n', thisDir);
