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
				<com:TActiveDropDownList ID="Level" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
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
				var mainForm = Prado.Validation.getForm();
				var callback = <%=$this->ReloadJobs->ActiveControl->Javascript%>;
				if (Prado.Validation.managers[mainForm].getValidatorsWithError('JobGroup').length == 0) {
					SlideWindow.getObj('JobWindow').markAllChecked(false);
					callback.dispatch();
				}
			}
		</script>
		<div class="button">
			<com:BActiveButton ID="Status" Text="<%[ Job status ]%>" CausesValidation="false" OnClick="status" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();" CssClass="bbutton" />
			<com:TActiveLabel ID="DeleteButton"><com:BActiveButton ID="Delete" Text="<%[ Delete job ]%>" CausesValidation="false" OnClick="delete" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();" CssClass="bbutton" /> </com:TActiveLabel>
			<com:TActiveLabel ID="CancelButton"><com:BActiveButton ID="Cancel" Text="<%[ Cancel job ]%>" CausesValidation="false" OnClick="cancel" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();" CssClass="bbutton" /> </com:TActiveLabel>
			<com:BActiveButton ID="Run" Text="<%[ Run job again ]%>" ValidationGroup="JobGroup" CausesValidation="true" OnClick="run_again" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();"/>
		</div>
		<div class="text small"><%[ Console status ]%></div>
		<div class="field-full" style="min-height: 166px">
			<com:TActiveTextBox ID="Estimation" TextMode="MultiLine" CssClass="textbox-auto" Style="height: 145px" ReadOnly="true" />
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Accurate" Text="<%[ Accurate: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Accurate" AutoPostBack="false" /></div>
		</div>
		<div class="button">
			<com:BActiveButton ID="Estimate" Text="<%[ Estimate job ]%>" OnClick="estimate" ClientSide.OnSuccess="ConfigurationWindow.getObj('JobWindow').progress(false);job_callback_func();" />
		</div>
	</com:TActivePanel>
</com:TContent>
