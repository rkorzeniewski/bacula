<%@ MasterClass="Application.Portlets.ConfigurationPanel"%>
<com:TContent ID="ConfigurationWindowContent">
	<com:TActivePanel DefaultButton="Run">
		<strong><%[ Job name: ]%> <com:TActiveLabel ID="JobName" /><com:TActiveLabel ID="JobID" Visible="false" /></strong>
		<hr />
		<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="JobGroup"
			AutoUpdate="true"
			Display="Dynamic"
			/>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Level" Text="<%[ Level: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Level" AutoPostBack="false" CssClass="textbox-auto">
					<prop:Attributes.onchange>
						var job_to_verify = $('<%=$this->JobToVerifyOptionsLine->ClientID%>');
						var verify_options = $('<%=$this->JobToVerifyOptionsLine->ClientID%>');
						var verify_by_job_name = $('<%=$this->JobToVerifyJobNameLine->ClientID%>');
						var verify_by_jobid = $('<%=$this->JobToVerifyJobIdLine->ClientID%>');
						var accurate = $('<%=$this->AccurateLine->ClientID%>');
						var estimate = $('<%=$this->EstimateLine->ClientID%>');
						var verify_current_opt = $('<%=$this->JobToVerifyOptions->ClientID%>').value;
						if(/^(<%=implode('|', $this->jobToVerify)%>)$/.test(this.value)) {
							accurate.hide();
							estimate.hide();
							verify_options.show();
							job_to_verify.show();
							if (verify_current_opt == 'jobid') {
								verify_by_job_name.hide();
								verify_by_jobid.show();
							} else if (verify_current_opt == 'jobname') {
								verify_by_job_name.show();
								verify_by_jobid.hide();
							}
						} else if (job_to_verify.visible()) {
							job_to_verify.hide();
							verify_options.hide();
							verify_by_job_name.hide();
							verify_by_jobid.hide();
							accurate.show();
							estimate.show();
						}
					</prop:Attributes.onchange>
				</com:TActiveDropDownList>
			</div>
		</div>
		<com:TActivePanel ID="JobToVerifyOptionsLine" CssClass="line">
			<div class="text"><com:TLabel ForControl="JobToVerifyOptions" Text="<%[ Verify option: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="JobToVerifyOptions" AutoPostBack="false" CssClass="textbox-auto">
					<prop:Attributes.onchange>
						var verify_by_job_name = $('<%=$this->JobToVerifyJobNameLine->ClientID%>');
						var verify_by_jobid = $('<%=$this->JobToVerifyJobIdLine->ClientID%>');
						if (this.value == 'jobname') {
							verify_by_jobid.hide();
							verify_by_job_name.show();
						} else if (this.value == 'jobid') {
							verify_by_job_name.hide();
							verify_by_jobid.show();
						} else {
							verify_by_job_name.hide();
							verify_by_jobid.hide();
						}
					</prop:Attributes.onchange>
				</com:TActiveDropDownList>
			</div>
		</com:TActivePanel>
		<com:TActivePanel ID="JobToVerifyJobNameLine" CssClass="line">
			<div class="text"><com:TLabel ForControl="JobToVerifyJobName" Text="<%[ Job to Verify: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="JobToVerifyJobName" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</com:TActivePanel>
		<com:TActivePanel ID="JobToVerifyJobIdLine" CssClass="line">
			<div class="text"><com:TLabel ForControl="JobToVerifyJobId" Text="<%[ JobId to Verify: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="JobToVerifyJobId" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="JobToVerifyJobIdValidator" ValidationGroup="JobGroup" ControlToValidate="JobToVerifyJobId" ErrorMessage="<%[ JobId to Verify value must be integer greather than 0. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="jobIdToVerifyValidator" />
			</div>
		</com:TActivePanel>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Client" Text="<%[ Client: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Client" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="FileSet" Text="<%[ FileSet: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="FileSet" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Pool" Text="<%[ Pool: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Pool" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Storage" Text="<%[ Storage: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Storage" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Priority" Text="<%[ Priority: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="Priority" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="PriorityValidator" ValidationGroup="JobGroup" ControlToValidate="Priority" ErrorMessage="<%[ Priority value must be integer greather than 0. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="priorityValidator" />
			</div>
		</div>
		<com:TCallback ID="ReloadJobs" OnCallback="Page.JobWindow.prepareData" ClientSide.OnComplete="SlideWindow.getObj('JobWindow').setLoadRequest();" />
		<script type="text/javascript">
			var job_callback_func = function() {
				/*
				 * Check if Job list window is open and if any checkbox from actions is not checked.
				 * If yes, then is possible to refresh Job list window.
				 */
				if(SlideWindow.getObj('JobWindow').isWindowOpen() === true && SlideWindow.getObj('JobWindow').areActionsOpen() === false) {
					var mainForm = Prado.Validation.getForm();
					var callback = <%=$this->ReloadJobs->ActiveControl->Javascript%>;
					if (Prado.Validation.managers[mainForm].getValidatorsWithError('JobGroup').length == 0) {
						SlideWindow.getObj('JobWindow').markAllChecked(false);
						callback.dispatch();
					}
				}
			}
		</script>
		<div class="button">
			<com:BActiveButton ID="Status" Text="<%[ Job status ]%>" CausesValidation="false" OnClick="status" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();" CssClass="bbutton" />
			<com:TActiveLabel ID="DeleteButton"><com:BActiveButton ID="Delete" Text="<%[ Delete job ]%>" CausesValidation="false" OnClick="delete" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();" CssClass="bbutton" /> </com:TActiveLabel>
			<com:TActiveLabel ID="CancelButton"><com:BActiveButton ID="Cancel" Text="<%[ Cancel job ]%>" CausesValidation="false" OnClick="cancel" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();" CssClass="bbutton" /> </com:TActiveLabel>
			<com:BActiveButton ID="Run" Text="<%[ Run job again ]%>" ValidationGroup="JobGroup" CausesValidation="true" OnClick="run_again" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();oMonitor();"/>
		</div>
		<com:TCallback ID="RefreshStatus" OnCallback="status" ClientSide.OnComplete="status_callback_timeout = setTimeout('status_callback_func()', 10000);" />
		<script type="text/javascript">
			var status_callback_timeout;
			var status_prev = false;
			var status_callback_func = function() {
				if(status_callback_timeout) {
					clearTimeout(status_callback_timeout);
				}
				if($('<%=$this->getID()%>configuration').visible() && ($('<%=$this->RefreshStart->ClientID%>').value === 'true' || status_prev === true)) {
					status_prev = ($('<%=$this->RefreshStart->ClientID%>').value === 'true');
					var callback = <%=$this->RefreshStatus->ActiveControl->Javascript%>;
					oMonitor();
					callback.dispatch();
				} else {
					status_prev = false;
				}
			}
		</script>
		<com:TActiveHiddenField ID="RefreshStart" />
		<div class="text small"><%[ Console status ]%></div>
		<div class="field-full" style="min-height: 166px">
			<com:TActiveTextBox ID="Estimation" TextMode="MultiLine" CssClass="textbox-auto" Style="height: 145px" ReadOnly="true" />
		</div>
		<com:TActivePanel ID="AccurateLine" CssClass="line">
			<div class="text"><com:TLabel ForControl="Accurate" Text="<%[ Accurate: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Accurate" AutoPostBack="false" /></div>
		</com:TActivePanel>
		<com:TActivePanel ID="EstimateLine" CssClass="button">
			<com:BActiveButton ID="Estimate" Text="<%[ Estimate job ]%>" OnClick="estimate" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();" />
		</com:TActivePanel>
	</com:TActivePanel>
</com:TContent>
