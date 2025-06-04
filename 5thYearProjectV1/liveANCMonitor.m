function liveANCMonitor()
    % Enhanced ANC monitoring with multiple plots
    % Serial port configuration
    portName = 'COM11';      % Change to your ESP32's COM port
    baudRate = 115200;
    maxPoints = 2000;        % Number of points to display
    updateInterval = 0.05;   % Update interval in seconds (50ms)
    
    % Initialize figure with subplots
    fig = figure('Name', 'LMS Adaptive Noise Cancellation Monitor', ...
                 'NumberTitle', 'off', ...
                 'Color', 'white', ...
                 'Position', [100, 100, 1200, 800]);
    
    % Create subplots
    subplot1 = subplot(2,2,1);
    grid(subplot1, 'on');
    hold(subplot1, 'on');
    title(subplot1, 'RMSE Convergence');
    xlabel(subplot1, 'Iteration (x1000)');
    ylabel(subplot1, 'RMSE');
    
    subplot2 = subplot(2,2,2);
    grid(subplot2, 'on');
    hold(subplot2, 'on');
    title(subplot2, 'Filter Coefficient Magnitude');
    xlabel(subplot2, 'Time (s)');
    ylabel(subplot2, 'Sum of |Coefficients|');
    
    subplot3 = subplot(2,2,3);
    grid(subplot3, 'on');
    hold(subplot3, 'on');
    title(subplot3, 'RMSE vs Time');
    xlabel(subplot3, 'Time (s)');
    ylabel(subplot3, 'RMSE');
    
    subplot4 = subplot(2,2,4);
    title(subplot4, 'System Status');
    axis(subplot4, 'off');
    
    % Initialize data buffers
    timestamps = NaN(maxPoints, 1);
    iterations = NaN(maxPoints, 1);
    rmseValues = NaN(maxPoints, 1);
    filterSums = NaN(maxPoints, 1);
    pointCount = 0;
    startTime = 0;
    
    % Create plot objects
    hPlot1 = plot(subplot1, iterations/1000, rmseValues, ...
                'LineWidth', 1.5, 'Color', [0 0.4470 0.7410]);
    hPlot2 = plot(subplot2, timestamps, filterSums, ...
                'LineWidth', 1.5, 'Color', [0.8500 0.3250 0.0980]);
    hPlot3 = plot(subplot3, timestamps, rmseValues, ...
                'LineWidth', 1.5, 'Color', [0.9290 0.6940 0.1250]);
    
    % Status text
    statusText = text(0.1, 0.8, 'Connecting...', 'Parent', subplot4, ...
                     'FontSize', 12, 'FontWeight', 'bold');
    metricsText = text(0.1, 0.6, '', 'Parent', subplot4, ...
                      'FontSize', 10);
    instructText = text(0.1, 0.2, ['Instructions:\n', ...
                                  '1. Ensure ESP32 is connected\n', ...
                                  '2. Generate noise near reference mic\n', ...
                                  '3. Watch RMSE decrease as filter adapts\n', ...
                                  '4. Close figure to stop monitoring'], ...
                       'Parent', subplot4, 'FontSize', 9);
    
    try
        % Initialize serial port
        % Use legacy serial interface (compatible with older MATLAB versions)
        s = serial(portName, 'BaudRate', baudRate);
        s.Terminator = 'LF';
        s.InputBufferSize = 4096;
        s.Timeout = 1;
        fopen(s);
        
        set(statusText, 'String', sprintf('Connected to %s', portName), 'Color', 'green');
        fprintf('Connected to %s at %d baud\n', portName, baudRate);
        
        % Clear any existing data
        pause(0.5);
        while s.BytesAvailable > 0
            fgetl(s);
        end
        
        % Main monitoring loop
        while ishandle(fig)
            tic;
            
            % Read available data
            dataReceived = false;
            lines = {};
            while s.BytesAvailable > 0
                lines{end+1} = fgetl(s);
                dataReceived = true;
            end
            
            if dataReceived
                for lineIdx = 1:length(lines)
                    try
                        lineStr = lines{lineIdx};
                        
                        % Skip header and empty lines
                        if isempty(lineStr) || ~isempty(strfind(lineStr, 'Time')) || ~isempty(strfind(lineStr, 'ANC'))
                            continue;
                        end
                        
                        % Parse CSV data
                        parts = strsplit(lineStr, ',');
                        if length(parts) >= 3
                            time_ms = str2double(parts{1});
                            iteration = str2double(parts{2});
                            rmse = str2double(parts{3});
                            
                            % Optional filter sum (if available)
                            filterSum = 0;
                            if length(parts) >= 4
                                filterSum = str2double(parts{4});
                            end
                            
                            if ~isnan(time_ms) && ~isnan(iteration) && ~isnan(rmse)
                                if startTime == 0
                                    startTime = time_ms;
                                end
                                
                                % Update data buffers
                                pointCount = pointCount + 1;
                                currentTime = (time_ms - startTime) / 1000; % Convert to seconds
                                
                                if pointCount > maxPoints
                                    % Shift arrays
                                    timestamps = circshift(timestamps, -1);
                                    iterations = circshift(iterations, -1);
                                    rmseValues = circshift(rmseValues, -1);
                                    filterSums = circshift(filterSums, -1);
                                    timestamps(end) = currentTime;
                                    iterations(end) = iteration;
                                    rmseValues(end) = rmse;
                                    filterSums(end) = filterSum;
                                else
                                    timestamps(pointCount) = currentTime;
                                    iterations(pointCount) = iteration;
                                    rmseValues(pointCount) = rmse;
                                    filterSums(pointCount) = filterSum;
                                end
                                
                                % Update plots
                                validPoints = find(~isnan(timestamps));
                                if ~isempty(validPoints)
                                    % RMSE vs Iteration
                                    set(hPlot1, 'XData', iterations(validPoints)/1000, ...
                                               'YData', rmseValues(validPoints));
                                    
                                    % Filter coefficients vs Time
                                    set(hPlot2, 'XData', timestamps(validPoints), ...
                                               'YData', filterSums(validPoints));
                                    
                                    % RMSE vs Time
                                    set(hPlot3, 'XData', timestamps(validPoints), ...
                                               'YData', rmseValues(validPoints));
                                    
                                    % Auto-scale axes
                                    currentRMSE = rmseValues(validPoints);
                                    currentFilter = filterSums(validPoints);
                                    currentTimes = timestamps(validPoints);
                                    currentIter = iterations(validPoints);
                                    
                                    % Update axis limits
                                    xlim(subplot1, [min(currentIter)/1000, max(currentIter)/1000]);
                                    ylim(subplot1, [0, max(1, max(currentRMSE)*1.1)]);
                                    
                                    xlim(subplot2, [min(currentTimes), max(currentTimes)]);
                                    ylim(subplot2, [0, max(1, max(currentFilter)*1.1)]);
                                    
                                    xlim(subplot3, [min(currentTimes), max(currentTimes)]);
                                    ylim(subplot3, [0, max(1, max(currentRMSE)*1.1)]);
                                    
                                    % Update status
                                    avgRMSE = mean(currentRMSE(max(1, end-50):end));
                                    set(metricsText, 'String', sprintf(['Current RMSE: %.4f\n', ...
                                                                       'Avg RMSE (50 samples): %.4f\n', ...
                                                                       'Iterations: %d\n', ...
                                                                       'Runtime: %.1f s\n', ...
                                                                       'Filter Activity: %.2f'], ...
                                                                      rmse, avgRMSE, iteration, currentTime, filterSum));
                                    
                                    % Convergence indicator
                                    if avgRMSE < 0.1
                                        set(statusText, 'String', 'Status: CONVERGED', 'Color', 'green');
                                    elseif avgRMSE < 0.3
                                        set(statusText, 'String', 'Status: CONVERGING', 'Color', 'orange');
                                    else
                                        set(statusText, 'String', 'Status: ADAPTING', 'Color', 'red');
                                    end
                                end
                            end
                        end
                    catch ME
                        fprintf('Error processing line: %s\n', ME.message);
                    end
                end
            end
            
            % Control update rate
            elapsed = toc;
            if elapsed < updateInterval
                pause(updateInterval - elapsed);
            end
            drawnow;
        end
        
    catch ME
        fprintf('Error: %s\n', ME.message);
        set(statusText, 'String', 'ERROR: Connection failed', 'Color', 'red');
    end
    
    % Cleanup
    try
        if exist('s', 'var') && isvalid(s)
            fclose(s);
            delete(s);
        end
        fprintf('Serial connection closed\n');
    catch
        % Ignore cleanup errors
    end
end